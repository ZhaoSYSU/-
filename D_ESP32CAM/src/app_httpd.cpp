#include <Arduino.h>
#include <WiFi.h>

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp32-hal-ledc.h"
#include "web_ui.h"

#include <lwip/sockets.h>
#include <lwip/tcp.h>

// ========================================
// 板载补光灯
// ========================================
#define CONFIG_LED_ILLUMINATOR_ENABLED 1

#if CONFIG_LED_ILLUMINATOR_ENABLED

static constexpr uint8_t LED_LEDC_CHANNEL = 2;
static constexpr uint8_t LED_MAX_INTENSITY = 255;

static int ledDuty = 0;
static bool isStreaming = false;

#endif

// ========================================
// MJPEG边界与响应头
// ========================================
#define PART_BOUNDARY "123456789000000000000987654321"

static const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

/*
 * 将边界和JPEG头合并成一次发送，
 * 每一帧只需要：
 *
 * 1. 发送帧头
 * 2. 发送JPEG数据
 */
static const char *STREAM_PART =
    "\r\n--" PART_BOUNDARY "\r\n"
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n\r\n";

static httpd_handle_t cameraHttpd = nullptr;
static httpd_handle_t streamHttpd = nullptr;

// 网页可设置的额外等待，默认必须为0
static volatile uint16_t streamDelayMs = 0;

// ========================================
// 补光灯控制
// ========================================
#if CONFIG_LED_ILLUMINATOR_ENABLED

static void enableLed(bool enabled)
{
    int duty = enabled ? ledDuty : 0;

    if (duty < 0)
    {
        duty = 0;
    }

    if (duty > LED_MAX_INTENSITY)
    {
        duty = LED_MAX_INTENSITY;
    }

    ledcWrite(LED_LEDC_CHANNEL, duty);
}

#endif

// ========================================
// 网页首页
// ========================================
static esp_err_t indexHandler(httpd_req_t *request)
{
    httpd_resp_set_type(
        request,
        "text/html; charset=utf-8"
    );

    httpd_resp_set_hdr(
        request,
        "Cache-Control",
        "no-store, no-cache, must-revalidate"
    );

    return httpd_resp_send(
        request,
        CONTROL_PAGE_HTML,
        HTTPD_RESP_USE_STRLEN
    );
}

// ========================================
// 单张抓拍
// ========================================
static esp_err_t captureHandler(httpd_req_t *request)
{
#if CONFIG_LED_ILLUMINATOR_ENABLED
    if (ledDuty > 0)
    {
        enableLed(true);

        // 只对单张抓拍等待，实时流不增加此延迟
        delay(30);
    }
#endif

    camera_fb_t *frame = esp_camera_fb_get();

#if CONFIG_LED_ILLUMINATOR_ENABLED
    if (ledDuty > 0)
    {
        enableLed(false);
    }
#endif

    if (frame == nullptr)
    {
        httpd_resp_send_500(request);
        return ESP_FAIL;
    }

    if (frame->format != PIXFORMAT_JPEG)
    {
        esp_camera_fb_return(frame);
        httpd_resp_send_500(request);
        return ESP_FAIL;
    }

    httpd_resp_set_type(request, "image/jpeg");

    httpd_resp_set_hdr(
        request,
        "Content-Disposition",
        "inline; filename=capture.jpg"
    );

    httpd_resp_set_hdr(
        request,
        "Access-Control-Allow-Origin",
        "*"
    );

    httpd_resp_set_hdr(
        request,
        "Cache-Control",
        "no-store"
    );

    const esp_err_t result = httpd_resp_send(
        request,
        reinterpret_cast<const char *>(frame->buf),
        frame->len
    );

    esp_camera_fb_return(frame);

    return result;
}

// ========================================
// MJPEG实时视频流
// ========================================
static esp_err_t streamHandler(httpd_req_t *request)
{
    esp_err_t result = httpd_resp_set_type(
        request,
        STREAM_CONTENT_TYPE
    );

    if (result != ESP_OK)
    {
        return result;
    }

    httpd_resp_set_hdr(
        request,
        "Access-Control-Allow-Origin",
        "*"
    );

    httpd_resp_set_hdr(
        request,
        "Cache-Control",
        "no-store, no-cache, must-revalidate"
    );

    httpd_resp_set_hdr(
        request,
        "Pragma",
        "no-cache"
    );

    /*
     * 关闭TCP Nagle算法。
     *
     * 避免较小的HTTP数据块等待合并后再发送，
     * 有助于降低MJPEG帧间抖动。
     */
    const int socketFd = httpd_req_to_sockfd(request);

    if (socketFd >= 0)
    {
        int enabled = 1;

        setsockopt(
            socketFd,
            IPPROTO_TCP,
            TCP_NODELAY,
            &enabled,
            sizeof(enabled)
        );

        /*
         * 增大发送缓冲区。
         * 实际值可能受lwIP配置上限限制。
         */
        int sendBufferSize = 24 * 1024;

        setsockopt(
            socketFd,
            SOL_SOCKET,
            SO_SNDBUF,
            &sendBufferSize,
            sizeof(sendBufferSize)
        );
    }

#if CONFIG_LED_ILLUMINATOR_ENABLED
    isStreaming = true;

    if (ledDuty > 0)
    {
        enableLed(true);
    }
#endif

    /*
     * 帧头很短，96字节足够。
     */
    char frameHeader[96];

    while (true)
    {
        camera_fb_t *frame = esp_camera_fb_get();

        if (frame == nullptr)
        {
            result = ESP_FAIL;
            break;
        }

        /*
         * 本工程固定摄像头直接输出JPEG，
         * 不允许在HTTP任务中执行二次JPEG压缩。
         */
        if (frame->format != PIXFORMAT_JPEG)
        {
            esp_camera_fb_return(frame);
            result = ESP_FAIL;
            break;
        }

        const int headerLength = snprintf(
            frameHeader,
            sizeof(frameHeader),
            STREAM_PART,
            static_cast<unsigned int>(frame->len)
        );

        if (
            headerLength <= 0 ||
            headerLength >= static_cast<int>(sizeof(frameHeader))
        )
        {
            esp_camera_fb_return(frame);
            result = ESP_FAIL;
            break;
        }

        // 第一次发送：MJPEG边界和当前帧头
        result = httpd_resp_send_chunk(
            request,
            frameHeader,
            headerLength
        );

        if (result == ESP_OK)
        {
            // 第二次发送：JPEG帧本体
            result = httpd_resp_send_chunk(
                request,
                reinterpret_cast<const char *>(frame->buf),
                frame->len
            );
        }

        /*
         * 必须在发送完成后立即归还Frame Buffer，
         * 让摄像头驱动尽快继续采集。
         */
        esp_camera_fb_return(frame);
        frame = nullptr;

        if (result != ESP_OK)
        {
            break;
        }

        /*
         * 不使用 vTaskDelay(1)：
         * 某些配置中一个FreeRTOS Tick可能达到10 ms，
         * 会人为限制最高帧率。
         *
         * taskYIELD只主动让出一次调度机会，
         * 不强制等待固定时间。
         */
        taskYIELD();

        if (streamDelayMs > 0)
        {
            delay(streamDelayMs);
        }
    }

    // 结束分块响应
    httpd_resp_send_chunk(request, nullptr, 0);

#if CONFIG_LED_ILLUMINATOR_ENABLED
    isStreaming = false;
    enableLed(false);
#endif

    return result;
}

// ========================================
// 读取GET参数
// ========================================
static esp_err_t parseGetQuery(
    httpd_req_t *request,
    char **outputBuffer
)
{
    const size_t queryLength =
        httpd_req_get_url_query_len(request) + 1;

    if (queryLength <= 1)
    {
        httpd_resp_send_404(request);
        return ESP_FAIL;
    }

    char *buffer =
        static_cast<char *>(malloc(queryLength));

    if (buffer == nullptr)
    {
        httpd_resp_send_500(request);
        return ESP_FAIL;
    }

    if (
        httpd_req_get_url_query_str(
            request,
            buffer,
            queryLength
        ) != ESP_OK
    )
    {
        free(buffer);
        httpd_resp_send_404(request);
        return ESP_FAIL;
    }

    *outputBuffer = buffer;
    return ESP_OK;
}

// ========================================
// 网页参数控制
// ========================================
static esp_err_t commandHandler(httpd_req_t *request)
{
    char *queryBuffer = nullptr;
    char variable[32] = {};
    char value[32] = {};

    if (parseGetQuery(request, &queryBuffer) != ESP_OK)
    {
        return ESP_FAIL;
    }

    const bool validQuery =
        httpd_query_key_value(
            queryBuffer,
            "var",
            variable,
            sizeof(variable)
        ) == ESP_OK &&
        httpd_query_key_value(
            queryBuffer,
            "val",
            value,
            sizeof(value)
        ) == ESP_OK;

    free(queryBuffer);

    if (!validQuery)
    {
        httpd_resp_send_404(request);
        return ESP_FAIL;
    }

    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor == nullptr)
    {
        httpd_resp_send_500(request);
        return ESP_FAIL;
    }

    const int requestedValue = atoi(value);
    int sensorResult = 0;

    if (strcmp(variable, "framesize") == 0)
    {
        if (sensor->pixformat != PIXFORMAT_JPEG)
        {
            sensorResult = -1;
        }
        else
        {
            /*
             * 限制为QQVGA～VGA。
             * 防止误选高分辨率导致视频长时间卡顿。
             */
            const int safeFrameSize = constrain(
                requestedValue,
                static_cast<int>(FRAMESIZE_QQVGA),
                static_cast<int>(FRAMESIZE_VGA)
            );

            sensorResult = sensor->set_framesize(
                sensor,
                static_cast<framesize_t>(safeFrameSize)
            );
        }
    }
    else if (strcmp(variable, "quality") == 0)
    {
        sensorResult = sensor->set_quality(
            sensor,
            constrain(requestedValue, 10, 63)
        );
    }
    else if (strcmp(variable, "brightness") == 0)
    {
        sensorResult = sensor->set_brightness(
            sensor,
            constrain(requestedValue, -2, 2)
        );
    }
    else if (strcmp(variable, "contrast") == 0)
    {
        sensorResult = sensor->set_contrast(
            sensor,
            constrain(requestedValue, -2, 2)
        );
    }
    else if (strcmp(variable, "saturation") == 0)
    {
        sensorResult = sensor->set_saturation(
            sensor,
            constrain(requestedValue, -2, 2)
        );
    }
    else if (strcmp(variable, "awb") == 0)
    {
        sensorResult =
            sensor->set_whitebal(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "awb_gain") == 0)
    {
        sensorResult =
            sensor->set_awb_gain(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "aec") == 0)
    {
        sensorResult =
            sensor->set_exposure_ctrl(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "aec2") == 0)
    {
        sensorResult =
            sensor->set_aec2(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "ae_level") == 0)
    {
        sensorResult = sensor->set_ae_level(
            sensor,
            constrain(requestedValue, -2, 2)
        );
    }
    else if (strcmp(variable, "agc") == 0)
    {
        sensorResult =
            sensor->set_gain_ctrl(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "hmirror") == 0)
    {
        sensorResult =
            sensor->set_hmirror(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "vflip") == 0)
    {
        sensorResult =
            sensor->set_vflip(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "wpc") == 0)
    {
        sensorResult =
            sensor->set_wpc(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "raw_gma") == 0)
    {
        sensorResult =
            sensor->set_raw_gma(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "lenc") == 0)
    {
        sensorResult =
            sensor->set_lenc(sensor, requestedValue != 0);
    }
    else if (strcmp(variable, "stream_delay") == 0)
    {
        streamDelayMs = static_cast<uint16_t>(
            constrain(requestedValue, 0, 200)
        );
    }
#if CONFIG_LED_ILLUMINATOR_ENABLED
    else if (strcmp(variable, "led_intensity") == 0)
    {
        ledDuty = constrain(
            requestedValue,
            0,
            static_cast<int>(LED_MAX_INTENSITY)
        );

        if (isStreaming)
        {
            enableLed(ledDuty > 0);
        }
    }
#endif
    else
    {
        sensorResult = -1;
    }

    if (sensorResult < 0)
    {
        httpd_resp_send_500(request);
        return ESP_FAIL;
    }

    httpd_resp_set_hdr(
        request,
        "Access-Control-Allow-Origin",
        "*"
    );

    return httpd_resp_send(request, nullptr, 0);
}

// ========================================
// 返回当前状态给网页
// ========================================
static esp_err_t statusHandler(httpd_req_t *request)
{
    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor == nullptr)
    {
        httpd_resp_send_500(request);
        return ESP_FAIL;
    }

    static char jsonResponse[1024];

    const int responseLength = snprintf(
        jsonResponse,
        sizeof(jsonResponse),

        "{"
        "\"xclk\":%u,"
        "\"pixformat\":%u,"
        "\"framesize\":%u,"
        "\"quality\":%u,"
        "\"brightness\":%d,"
        "\"contrast\":%d,"
        "\"saturation\":%d,"
        "\"awb\":%u,"
        "\"awb_gain\":%u,"
        "\"aec\":%u,"
        "\"aec2\":%u,"
        "\"ae_level\":%d,"
        "\"agc\":%u,"
        "\"wpc\":%u,"
        "\"raw_gma\":%u,"
        "\"lenc\":%u,"
        "\"hmirror\":%u,"
        "\"vflip\":%u,"
        "\"led_intensity\":%d,"
        "\"stream_delay\":%u,"
        "\"rssi\":%d,"
        "\"clients\":%u,"
        "\"free_heap\":%u,"
        "\"free_psram\":%u"
        "}",

        sensor->xclk_freq_hz / 1000000,
        sensor->pixformat,
        sensor->status.framesize,
        sensor->status.quality,
        sensor->status.brightness,
        sensor->status.contrast,
        sensor->status.saturation,
        sensor->status.awb,
        sensor->status.awb_gain,
        sensor->status.aec,
        sensor->status.aec2,
        sensor->status.ae_level,
        sensor->status.agc,
        sensor->status.wpc,
        sensor->status.raw_gma,
        sensor->status.lenc,
        sensor->status.hmirror,
        sensor->status.vflip,

#if CONFIG_LED_ILLUMINATOR_ENABLED
        ledDuty,
#else
        -1,
#endif

        streamDelayMs,

        /*
         * SoftAP模式下 WiFi.RSSI() 不一定能返回客户端RSSI。
         * 保留字段只是为了兼容当前网页。
         */
        WiFi.RSSI(),

        WiFi.softAPgetStationNum(),
        ESP.getFreeHeap(),
        ESP.getFreePsram()
    );

    if (
        responseLength <= 0 ||
        responseLength >= static_cast<int>(sizeof(jsonResponse))
    )
    {
        httpd_resp_send_500(request);
        return ESP_FAIL;
    }

    httpd_resp_set_type(
        request,
        "application/json"
    );

    httpd_resp_set_hdr(
        request,
        "Access-Control-Allow-Origin",
        "*"
    );

    httpd_resp_set_hdr(
        request,
        "Cache-Control",
        "no-store"
    );

    return httpd_resp_send(
        request,
        jsonResponse,
        responseLength
    );
}

// ========================================
// 启动HTTP服务器
// ========================================
void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;

    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    /*
     * 网页页面较大，适当增加任务栈。
     */
    config.stack_size = 8192;

#if !CONFIG_FREERTOS_UNICORE
    /*
     * ESP32 Wi-Fi协议栈主要运行在Core 0。
     * 将HTTP服务放到Core 1，减少直接竞争。
     */
    config.core_id = 1;
#endif

    // ------------------------------
    // 端口80：网页、状态、控制、抓拍
    // ------------------------------
    httpd_uri_t indexUri = {};
    indexUri.uri = "/";
    indexUri.method = HTTP_GET;
    indexUri.handler = indexHandler;

    httpd_uri_t statusUri = {};
    statusUri.uri = "/status";
    statusUri.method = HTTP_GET;
    statusUri.handler = statusHandler;

    httpd_uri_t controlUri = {};
    controlUri.uri = "/control";
    controlUri.method = HTTP_GET;
    controlUri.handler = commandHandler;

    httpd_uri_t captureUri = {};
    captureUri.uri = "/capture";
    captureUri.method = HTTP_GET;
    captureUri.handler = captureHandler;

    if (httpd_start(&cameraHttpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(cameraHttpd, &indexUri);
        httpd_register_uri_handler(cameraHttpd, &statusUri);
        httpd_register_uri_handler(cameraHttpd, &controlUri);
        httpd_register_uri_handler(cameraHttpd, &captureUri);
    }
    else
    {
        Serial.println("Failed to start HTTP server on port 80.");
    }

    // ------------------------------
    // 端口81：专用MJPEG视频流
    // ------------------------------
    config.server_port += 1;
    config.ctrl_port += 1;

    httpd_uri_t streamUri = {};
    streamUri.uri = "/stream";
    streamUri.method = HTTP_GET;
    streamUri.handler = streamHandler;

    if (httpd_start(&streamHttpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(streamHttpd, &streamUri);
    }
    else
    {
        Serial.println("Failed to start stream server on port 81.");
    }
}

// ========================================
// 初始化板载补光灯PWM
// ========================================
void setupLedFlash(int pin)
{
#if CONFIG_LED_ILLUMINATOR_ENABLED
    ledcSetup(
        LED_LEDC_CHANNEL,
        5000,
        8
    );

    ledcAttachPin(
        pin,
        LED_LEDC_CHANNEL
    );

    ledcWrite(
        LED_LEDC_CHANNEL,
        0
    );
#else
    (void)pin;
#endif
}