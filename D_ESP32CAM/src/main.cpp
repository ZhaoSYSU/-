#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>

// ========================================
// 摄像头型号：AI Thinker ESP32-CAM
// ========================================
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// ========================================
// ESP32-CAM 自建热点配置
// ========================================
static const char *AP_SSID     = "ESP32CAM-BALL";
static const char *AP_PASSWORD = "ticar2026";

// 建议信道优先测试 1、6、11
static constexpr uint8_t AP_CHANNEL = 6;

// 只允许一台电脑连接
static constexpr uint8_t AP_MAX_CONNECTIONS = 1;

// 固定热点地址
IPAddress apIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// app_httpd.cpp 中实现
void startCameraServer();
void setupLedFlash(int pin);

/**
 * 停止程序并持续输出错误提示。
 *
 * setup() 中直接 return 后，Arduino 仍然会继续进入 loop()，
 * 因此关键初始化失败时使用阻塞方式更合适。
 */
static void haltSystem(const char *message)
{
    Serial.println(message);

    while (true)
    {
        delay(1000);
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(false);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32-CAM Low-Latency Stream");
    Serial.println("================================");

    const bool hasPSRAM = psramFound();

    Serial.printf(
        "PSRAM: %s\n",
        hasPSRAM ? "FOUND" : "NOT FOUND"
    );

    // ========================================
    // 1. 摄像头初始化配置
    // ========================================
    camera_config_t config = {};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk  = XCLK_GPIO_NUM;
    config.pin_pclk  = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href  = HREF_GPIO_NUM;

    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn  = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    // ESP32-CAM规格书图传测试条件也是20 MHz
    config.xclk_freq_hz = 20000000;

    // MJPEG图传必须优先使用摄像头直接输出JPEG
    config.pixel_format = PIXFORMAT_JPEG;

   /*
 * 初始化时按网页允许的最高分辨率预留Frame Buffer。
 * 当前网页最高允许VGA，所以初始化使用VGA。
 *
 * 初始化完成后，再把实际默认分辨率切换回QQVGA。
 */
config.frame_size = hasPSRAM
    ? FRAMESIZE_VGA
    : FRAMESIZE_QQVGA;

config.jpeg_quality = hasPSRAM
    ? 24
    : 35;

    if (hasPSRAM)
    {
        /*
         * 双帧缓冲：
         * 一块缓冲区发送时，另一块可继续采集。
         */
        config.fb_count = 2;

        /*
         * 优先保留最新帧。
         * 网络短暂阻塞时丢弃旧帧，避免延迟不断累积。
         */
        config.grab_mode = CAMERA_GRAB_LATEST;

        config.fb_location = CAMERA_FB_IN_PSRAM;
    }
    else
    {
        /*
         * AI Thinker ESP32-CAM正常应检测到PSRAM。
         * 未检测到PSRAM时只能使用更保守配置。
         */
        config.fb_count = 1;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

    // ========================================
    // 2. 初始化摄像头
    // ========================================
    const esp_err_t cameraError = esp_camera_init(&config);

    if (cameraError != ESP_OK)
    {
        Serial.printf(
            "Camera init failed, error: 0x%x\n",
            cameraError
        );

        haltSystem("Check camera ribbon cable and 5V power.");
    }

    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor == nullptr)
    {
        haltSystem("Failed to get camera sensor.");
    }

    // ========================================
    // 3. OV2640图像参数
    // ========================================
    if (sensor->id.PID == OV2640_PID)
    {
        // 不使用灰度、负片等特殊效果
        sensor->set_special_effect(sensor, 0);

        // 基础颜色参数
        sensor->set_brightness(sensor, 0);
        sensor->set_contrast(sensor, 0);
        sensor->set_saturation(sensor, 1);

        /*
         * 这些功能主要在OV2640内部DSP执行。
         * 保留自动控制有利于小球颜色、亮度和曝光稳定。
         */
        sensor->set_whitebal(sensor, 1);
        sensor->set_awb_gain(sensor, 1);
        sensor->set_wb_mode(sensor, 0);

        sensor->set_exposure_ctrl(sensor, 1);
        sensor->set_aec2(sensor, 1);
        sensor->set_ae_level(sensor, 0);

        sensor->set_gain_ctrl(sensor, 1);
        sensor->set_gainceiling(sensor, GAINCEILING_8X);

        sensor->set_bpc(sensor, 0);
        sensor->set_wpc(sensor, 1);
        sensor->set_raw_gma(sensor, 1);
        sensor->set_lenc(sensor, 1);
        sensor->set_dcw(sensor, 1);

        /*
         * 上下翻转 + 水平镜像 = 旋转180°。
         */
        sensor->set_vflip(sensor, 0);
        sensor->set_hmirror(sensor, 0);
    }

    // 再次确认实际运行参数
    sensor->set_framesize(sensor, FRAMESIZE_QQVGA);
    sensor->set_quality(sensor, 30);

    // ========================================
    // 4. 初始化板载补光灯
    // ========================================
#if defined(LED_GPIO_NUM)
    setupLedFlash(LED_GPIO_NUM);
#endif

    // ========================================
    // 5. 创建ESP32-CAM热点
    // ========================================
    WiFi.persistent(false);

    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(300);

    if (!WiFi.mode(WIFI_AP))
    {
        haltSystem("Failed to enter WIFI_AP mode.");
    }

    if (!WiFi.softAPConfig(apIP, gateway, subnet))
    {
        haltSystem("SoftAP IP configuration failed.");
    }

    const bool apStarted = WiFi.softAP(
        AP_SSID,
        AP_PASSWORD,
        AP_CHANNEL,
        false,
        AP_MAX_CONNECTIONS
    );

    if (!apStarted)
    {
        haltSystem("SoftAP start failed.");
    }

    // 关闭Wi-Fi节能，减少周期性等待
    WiFi.setSleep(false);

    // 设置较高发射功率
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    delay(300);

    // ========================================
    // 6. 启动网页与MJPEG服务器
    // ========================================
    startCameraServer();

    const IPAddress currentIP = WiFi.softAPIP();

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32-CAM hotspot started");
    Serial.println("================================");

    Serial.print("SSID: ");
    Serial.println(AP_SSID);

    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);

    Serial.print("Channel: ");
    Serial.println(AP_CHANNEL);

    Serial.print("AP IP: ");
    Serial.println(currentIP);

    Serial.println();

    Serial.print("Control panel: http://");
    Serial.print(currentIP);
    Serial.println("/");

    Serial.print("Direct stream: http://");
    Serial.print(currentIP);
    Serial.println(":81/stream");

    Serial.print("Single capture: http://");
    Serial.print(currentIP);
    Serial.println("/capture");

    Serial.println();
    Serial.println("Default: QQVGA 160x120, JPEG Quality 30");
    Serial.println("Open only one stream client.");
    Serial.println("================================");
}

void loop()
{
    /*
     * 摄像头驱动、HTTP服务器和Wi-Fi协议栈
     * 均运行在各自的FreeRTOS任务中。
     *
     * 主循环保持空闲，不再周期性打印状态。
     */
    delay(1000);
}