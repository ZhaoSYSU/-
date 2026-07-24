/*
 * ================================================================
 *  LP_MSPM0G3507 双电机机器人平台 — 主程序 (含K230 UART接收)
 * ================================================================
 *
 *  【SysConfig 配置步骤】
 *    1. 打开 empty.syscfg
 *    2. 添加 UART 外设 → 选 UART1
 *       PA8 改为 RX (原AIN1→挪到PA5)
 *       PA9 改为 TX
 *       115200, 8N1, 开 RX 中断
 *    3. 电机A AIN1 从 PA8 改为 PA5（在 GPIO 里改）
 *    4. Save → 重新生成 ti_msp_dl_config.c/.h
 *
 *  【引脚接线总表】
 *
 *   [电机A — TB6612FNG 通道1]
 *     PA5   → AIN1   (方向控制, 从PA8挪过来)
 *     PA15  → AIN2   (方向控制)
 *     PA12  → PWMA   (TIMG0 CCP0)
 *
 *   [电机B — TB6612FNG 通道2]
 *     PB13  → BIN1   (方向控制)
 *     PB12  → BIN2   (方向控制)
 *     PA13  → PWMB   (TIMG0 CCP1)
 *
 *   [TB6612FNG 公共]
 *     PB24  → STBY
 *
 *   [编码器A — MG310]
 *     PA17 → E1A, PA18 → E1B
 *
 *   [编码器B — MG310]
 *     PB4  → E2A, PB15 → E2B
 *
 *   [OLED — SSD1306 SPI]
 *     PB9→SCLK, PB8→MOSI, PB3→RES, PB2→DC, PA27→CS
 *
 *   [K230 UART — 视觉数据]
 *     PA8  → UART1 RX  ← K230 GPIO40 (TXD)
 *     PA9  → UART1 TX  → K230 GPIO41 (RXD)
 *
 *   [调试] PA20→SWCLK, PA19→SWDIO
 *
 *   [定时器]
 *     TIMG0  → PWM 双路, 10kHz
 *     TIMG12 → 编码器轮询, 1ms
 *     TIMG6  → 速度+PI, 100ms
 *
 *   [编码参数]
 *     20PPR × 减速比13 × 2倍频 = 520CPR
 *     轮径48mm, 周长150.72mm
 *     速度 = pulse × 942 ÷ 325  (mm/s)
 * ================================================================
 */

#include "ti_msp_dl_config.h"
#include "oled.h"
#include "motor.h"
#include "encoder.h"

/* ======================== UART 帧接收 ======================== */

/* K230 视觉协议帧格式: [0xAA][CMD][DH][DL][CS][0x55] */
#define UART_FRAME_LEN  6
#define UART_HEADER     0xAA
#define UART_FOOTER     0x55
#define UART_TIMEOUT_MS 10    /* 帧超时(ms)，约 800000 cycles @80MHz */

/* 接收状态机 */
typedef enum {
    WAIT_HEADER = 0,
    WAIT_DATA
} UartRxState;

static volatile UartRxState rx_state = WAIT_HEADER;
static volatile uint8_t   rx_buf[UART_FRAME_LEN];
static volatile uint8_t   rx_idx;
static volatile uint32_t  rx_timeout;    /* 超时刻度计数 */
static volatile bool      rx_timeout_flag;

/* 解析结果 */
typedef struct {
    int16_t  deviation;    /* CMD 0x01: 巡线偏差 */
    uint8_t  digit;        /* CMD 0x02: 数字 1~8 */
    uint8_t  status;       /* CMD 0x03: 0=OK, 2=丢线 */
    int16_t  track;        /* CMD 0x04: 0=直, -80=左, 80=右 */
    bool     new_digit;    /* 有新数字 */
    bool     new_track;    /* 有新轨道 */
} VisionData;

static volatile VisionData gVis = {0};
static volatile bool gVisUpdated = false;

/* ---- 校验和 ---- */
static inline uint8_t calc_cs(uint8_t cmd, uint8_t dh, uint8_t dl)
{
    return (cmd + dh + dl) & 0xFF;
}

/* ---- 帧解析 (在中断里调用) ---- */
static void parse_frame(void)
{
    uint8_t cmd  = rx_buf[1];
    uint8_t dh   = rx_buf[2];
    uint8_t dl   = rx_buf[3];
    uint8_t cs   = rx_buf[4];
    uint8_t foot = rx_buf[5];

    if (foot != UART_FOOTER) return;
    if (calc_cs(cmd, dh, dl) != cs) return;

    switch (cmd) {
    case 0x01:   /* 巡线偏差 */
        gVis.deviation = (int16_t)((dh << 8) | dl);
        break;
    case 0x02:   /* 数字识别 */
        gVis.digit = dl;
        gVis.new_digit = true;
        break;
    case 0x03:   /* 视觉状态 */
        gVis.status = dl;
        break;
    case 0x04:   /* 轨道类型 */
        gVis.track = (int16_t)((dh << 8) | dl);
        gVis.new_track = true;
        break;
    default:
        break;
    }
    gVisUpdated = true;
}

/* ---- UART1 中断服务 ---- */
void UART_0_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST)) {
    case DL_UART_MAIN_IIDX_RX: {
        uint8_t byte = DL_UART_Main_receiveData(UART_0_INST);

        if (rx_state == WAIT_HEADER) {
            if (byte == UART_HEADER) {
                rx_buf[0] = byte;
                rx_idx = 1;
                rx_state = WAIT_DATA;
                rx_timeout = 0;
                rx_timeout_flag = false;
            }
        } else {
            rx_buf[rx_idx++] = byte;
            if (rx_idx >= UART_FRAME_LEN) {
                parse_frame();
                rx_state = WAIT_HEADER;
            }
        }
        break;
    }
    default:
        break;
    }
}

/* ---- 超时检测 (由 TIMG12 中断中调用, 每1ms) ---- */
void UART_CheckTimeout(void)
{
    if (rx_state == WAIT_DATA && !rx_timeout_flag) {
        if (++rx_timeout >= UART_TIMEOUT_MS) {
            rx_timeout_flag = true;
            rx_state = WAIT_HEADER;   /* 超时丢弃，重找帧头 */
        }
    }
}


/* ======================== 主程序 ======================== */

int main(void)
{
    SYSCFG_DL_init();
    Encoder_Init();

    /* 使能 UART1 接收中断 */
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);

    /* OLED */
    OLED_Init();
    OLED_Clear();
    OLED_Refresh();

    /* 电机初始 */
    Motor_Init();
    Motor_Enable();

    /* 巡线参数 */
    const int32_t kBaseSpeed = 40;    /* 直走基速 mm/s */
    const int32_t kTurnGain  = 2;     /* 偏差增益: 每像素偏差 → mm/s 差速 */

    while (1) {
        /* ---- OLED 显示 ---- */
        OLED_Clear();
        {
            char buf[16];
            int32_t spdA = Encoder_GetSpeed_A();
            int32_t spdB = Encoder_GetSpeed_B();

            OLED_ShowString(1, 0, "V A:");      /* 速度A */
            {
                int i = 0; int32_t v = spdA;
                if (v < 0) { buf[i++] = '-'; v = -v; }
                buf[i++] = '0' + ((v / 10000) % 10);
                buf[i++] = '0' + ((v / 1000)  % 10);
                buf[i++] = '0' + ((v / 100)   % 10);
                buf[i++] = '0' + ((v / 10)     % 10);
                buf[i++] = '0' + (v % 10);
                buf[i] = 0;
                OLED_ShowString(1, 40, buf);
            }

            OLED_ShowString(2, 0, "V B:");      /* 速度B */
            {
                int i = 0; int32_t v = spdB;
                if (v < 0) { buf[i++] = '-'; v = -v; }
                buf[i++] = '0' + ((v / 10000) % 10);
                buf[i++] = '0' + ((v / 1000)  % 10);
                buf[i++] = '0' + ((v / 100)   % 10);
                buf[i++] = '0' + ((v / 10)     % 10);
                buf[i++] = '0' + (v % 10);
                buf[i] = 0;
                OLED_ShowString(2, 40, buf);
            }

            /* 第三行: 视觉数据 */
            {
                char vbuf[22];
                int i = 0;
                vbuf[i++] = 'D'; vbuf[i++] = ':';
                {
                    int16_t d = gVis.deviation;
                    if (d < 0) { vbuf[i++] = '-'; d = -d; }
                    vbuf[i++] = '0' + (d / 100);
                    vbuf[i++] = '0' + (d / 10) % 10;
                    vbuf[i++] = '0' + (d % 10);
                }
                vbuf[i++] = ' ';
                vbuf[i++] = 'N'; vbuf[i++] = ':';
                vbuf[i++] = gVis.digit ? '0' + gVis.digit : '-';
                vbuf[i++] = ' ';
                vbuf[i++] = 'T'; vbuf[i++] = ':';
                if (gVis.track == 0)      { vbuf[i++]='S'; }
                else if (gVis.track < 0)  { vbuf[i++]='L'; }
                else if (gVis.track > 0)  { vbuf[i++]='R'; }
                else                       { vbuf[i++]='-'; }
                vbuf[i++] = ' ';
                vbuf[i++] = gVis.status==2 ? '!' : ' ';
                vbuf[i] = 0;
                OLED_ShowString(3, 0, vbuf);
            }
        }
        OLED_Refresh();

        /* ---- 消费视觉数据 ---- */
        if (gVis.status == 2) {
            /* 丢线 → 停车 */
            Motor_SetTarget(MOTOR_A, 0);
            Motor_SetTarget(MOTOR_B, 0);
        } else {
            /* 巡线: 偏差 → 差速 */
            int32_t diff = gVis.deviation * kTurnGain;
            Motor_SetTarget(MOTOR_A, kBaseSpeed + diff);
            Motor_SetTarget(MOTOR_B, kBaseSpeed - diff);
        }

        /* 新数字/轨道 → 交给状态机处理(暂存, 待C_Scheduler接入) */
        if (gVis.new_digit) {
            // TODO: 状态机判断是否目标病房
            gVis.new_digit = false;
        }
        if (gVis.new_track) {
            // TODO: 触发转弯/直行决策
            gVis.new_track = false;
        }

        delay_cycles(3200000);  /* ~40ms @80MHz → 主循环 ~25Hz */
    }
}
