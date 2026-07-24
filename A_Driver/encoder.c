/*
 *  encoder.c  --  双路编码器软件解码 + 转速计算
 *
 *  原理: 定时器中断每1ms轮询编码器A/B信号
 *  检测边沿变化 → 判断方向 → 累加脉冲数
 *  每100ms计算一次 RPM
 */
#include "ti_msp_dl_config.h"
#include "encoder.h"
#include "motor.h"

/* 编码器引脚 */
#define E1A_PORT   GPIOA
#define E1A_PIN    DL_GPIO_PIN_17
#define E1B_PORT   GPIOA
#define E1B_PIN    DL_GPIO_PIN_18

#define E2A_PORT  GPIOB
#define E2A_PIN   DL_GPIO_PIN_4
#define E2B_PORT  GPIOB
#define E2B_PIN   DL_GPIO_PIN_15

/* 编码器线数 (A相双沿计数, 2倍频) */
#define ENC_PPR    20    /* 编码器每转脉冲数 */
#define ENC_RATIO  13    /* 减速比 */
#define ENC_CPR    (ENC_PPR * ENC_RATIO * 2)  /* 520 = 轮子每转脉冲数 */

#define PI              3.14f
#define WHEEL_DIAMETER  48    /* mm */
#define WHEEL_CIRCUM    (PI * WHEEL_DIAMETER)  /* 150.72mm */

#define ENC_INVERT_A  0
#define ENC_INVERT_B  0
typedef struct {
    volatile int32_t  pulse_count;
    volatile int32_t  speed;       /* mm/s */
    GPIO_Regs        *port;
    uint32_t          pin_a;
    uint32_t          pin_b;
    unsigned char     last_a;      /* 上次确认的A值 */
    unsigned char     last_raw;    /* 上次原始A值(去抖) */
} Encoder_t;

static Encoder_t gEncA, gEncB;

/* ---- 软件解码: A相双沿 + B判方向 + 2次去抖 ---- */
static void Encoder_Decode(Encoder_t *enc)
{
    unsigned char a = (DL_GPIO_readPins(enc->port, enc->pin_a) != 0);
    unsigned char b = (DL_GPIO_readPins(enc->port, enc->pin_b) != 0);

    /* 去抖: 连续2次读到相同值才接受 */
    if (a == enc->last_raw) {
        if (a != enc->last_a) {           /* 确认的A相变化 */
            if (a != b) enc->pulse_count++;   /* A≠B → 正转 */
            else        enc->pulse_count--;   /* A=B → 反转 */
            enc->last_a = a;
        }
    }
    enc->last_raw = a;
}

/* ---- TIMG12 中断: 每1ms轮询编码器 ---- */
void TIMG12_IRQHandler(void)
{
    if (DL_TimerG_getPendingInterrupt(TIMG12) & DL_TIMER_IIDX_ZERO) {
        Encoder_Decode(&gEncA);
        Encoder_Decode(&gEncB);
    }
}

/* ---- TIMG6 中断: 每100ms计算速度并执行PID ---- */
void TIMG6_IRQHandler(void)
{
    if (DL_TimerG_getPendingInterrupt(TIMG6) & DL_TIMER_IIDX_ZERO) {
        int32_t pcA, pcB;
        /* 临界区: 禁止TIMG12抢占, 防止pulse_count读写竞争 */
        NVIC_DisableIRQ(TIMG12_INT_IRQn);
        pcA = gEncA.pulse_count;
        pcB = gEncB.pulse_count;
        gEncA.pulse_count = 0;
        gEncB.pulse_count = 0;
        NVIC_EnableIRQ(TIMG12_INT_IRQn);

        /* 速度(mm/s) = pulse_count × 150.72 / 520 / 0.1 */
        /*            = pulse_count × 942 / 325 */
        gEncA.speed = (int32_t)((int64_t)pcA * 942 / 325);
        gEncB.speed = (int32_t)((int64_t)pcB * 942 / 325);
#if ENC_INVERT_A
        gEncA.speed = -gEncA.speed;
#endif
#if ENC_INVERT_B
        gEncB.speed = -gEncB.speed;
#endif
        Motor_PID_Update(gEncA.speed, gEncB.speed);
    }
}

/* ---- 初始化 ---- */
void Encoder_Init(void)
{
    /* 编码器引脚: 输入 + 上拉 + 迟滞滤波 */
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM39,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_disableOutput(GPIOA, DL_GPIO_PIN_17);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM40,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_disableOutput(GPIOA, DL_GPIO_PIN_18);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM17,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_disableOutput(GPIOB, DL_GPIO_PIN_4);
    DL_GPIO_initDigitalInputFeatures(IOMUX_PINCM32,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_disableOutput(GPIOB, DL_GPIO_PIN_15);

    gEncA.port = E1A_PORT; gEncA.pin_a = E1A_PIN; gEncA.pin_b = E1B_PIN;
    gEncA.last_a = 0; gEncA.last_raw = 0; gEncA.pulse_count = 0; gEncA.speed = 0;
    gEncB.port = E2A_PORT; gEncB.pin_a = E2A_PIN; gEncB.pin_b = E2B_PIN;
    gEncB.last_a = 0; gEncB.last_raw = 0; gEncB.pulse_count = 0; gEncB.speed = 0;

    /* TIMG12: 1ms 编码器轮询 */
    DL_TimerG_reset(TIMG12);
    DL_TimerG_enablePower(TIMG12);
    delay_cycles(16);
    DL_TimerG_setClockConfig(TIMG12,
        &(DL_TimerG_ClockConfig){DL_TIMER_CLOCK_BUSCLK, DL_TIMER_CLOCK_DIVIDE_1, 0});
    DL_TimerG_initTimerMode(TIMG12,
        &(DL_TimerG_TimerConfig){
            .timerMode = DL_TIMER_TIMER_MODE_PERIODIC,
            .period = 32000, .startTimer = DL_TIMER_START,
            .genIntermInt = DL_TIMER_INTERM_INT_DISABLED, .counterVal = 0
        });
    DL_TimerG_enableClock(TIMG12);
    DL_TimerG_enableInterrupt(TIMG12, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(TIMG12_INT_IRQn);

    /* TIMG6: 100ms 速度计算+PID控制 (BUSCLK/256=125kHz, period=12500) */
    DL_TimerG_reset(TIMG6);
    DL_TimerG_enablePower(TIMG6);
    delay_cycles(16);
    DL_TimerG_setClockConfig(TIMG6,
        &(DL_TimerG_ClockConfig){DL_TIMER_CLOCK_BUSCLK, DL_TIMER_CLOCK_DIVIDE_1, 255});
    DL_TimerG_initTimerMode(TIMG6,
        &(DL_TimerG_TimerConfig){
            .timerMode = DL_TIMER_TIMER_MODE_PERIODIC_UP,
            .period = 12500, .startTimer = DL_TIMER_START,
            .genIntermInt = DL_TIMER_INTERM_INT_DISABLED, .counterVal = 0
        });
    DL_TimerG_enableClock(TIMG6);
    DL_TimerG_enableInterrupt(TIMG6, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(TIMG6_INT_IRQn);
}

int32_t Encoder_GetSpeed_A(void) { return gEncA.speed; }
int32_t Encoder_GetSpeed_B(void) { return gEncB.speed; }
int32_t Encoder_GetPulse_A(void) { return gEncA.pulse_count; }
int32_t Encoder_GetPulse_B(void) { return gEncB.pulse_count; }
