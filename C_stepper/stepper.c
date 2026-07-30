/*
 * stepper.c  --  步进电机控球实现 (MSPM0G3507)
 *
 * 主控板 ──UART──▶ 本板
 *   CMD 0x01: ball_x_mm (int16, 大端)
 *   CMD 0x05: track_type(1B) + speed_cms(1B)
 *
 * 本板: 双PID+前馈 → TIMG12 PWM → 步进电机 → 摆杆
 */
#include "stepper.h"
#include <math.h>
#include <string.h>
#include "ti_msp_dl_config.h"

/* ==================== 引脚 (可改) ==================== */
#define STEP_PWM_INST       TIMG12
#define STEP_DIR_L_PORT      GPIOA
#define STEP_DIR_L_PIN       DL_GPIO_PIN_5
#define STEP_DIR_R_PORT      GPIOA
#define STEP_DIR_R_PIN       DL_GPIO_PIN_6
#define STEP_PWM_L_CH        DL_TIMER_CC_0_INDEX
#define STEP_PWM_R_CH        DL_TIMER_CC_1_INDEX

#define UART_INST            UART_0_INST
#define UART_INT_IRQN        UART_0_INST_INT_IRQN

#define CONTROL_TIMER        TIMG6
#define CONTROL_PERIOD_MS    10

/* UART 帧 */
#define FRAME_HEADER  0xAAU
#define FRAME_FOOTER  0x55U

/* ==================== 全局 ==================== */
static PID_t     pid;
static TrackType track_type = TRACK_STRAIGHT;

static volatile int16_t  ball_x_mm;
static volatile bool     ball_updated;
static volatile uint8_t  car_track;
static volatile uint8_t  car_speed;
static volatile bool     car_updated;

/* UART 状态机 */
static uint8_t  rx_buf[6];
static uint8_t  rx_idx;
static bool     rx_synced;

/* ==================== 初始化 ==================== */
void Stepper_Init(void)
{
    pid.kp      = (float)PID_S_KP / PID_SCALE;
    pid.ki      = (float)PID_S_KI / PID_SCALE;
    pid.kd      = (float)PID_S_KD / PID_SCALE;
    pid.target  = 0;
    pid.error   = 0;
    pid.last_error = 0;
    pid.integral   = 0;
    pid.output     = 0;
    pid.output_int = 0;

    ball_x_mm    = 0; ball_updated = false;
    car_track    = 0; car_speed    = 0; car_updated  = false;
    track_type   = TRACK_STRAIGHT;
    rx_idx = 0; rx_synced = false;

    /* 使能 UART 中断 */
    NVIC_ClearPendingIRQ(UART_INT_IRQN);
    NVIC_EnableIRQ(UART_INT_IRQN);
}

/* ==================== UART 接收 (中断调) ==================== */
void Stepper_UART_RX(uint8_t byte)
{
    if (!rx_synced) {
        if (byte == FRAME_HEADER) {
            rx_buf[0] = byte; rx_idx = 1; rx_synced = true;
        }
        return;
    }
    rx_buf[rx_idx++] = byte;
    if (rx_idx >= 6) {
        rx_synced = false;
        if (rx_buf[5] != FRAME_FOOTER) return;
        if (((rx_buf[1] + rx_buf[2] + rx_buf[3]) & 0xFFU) != rx_buf[4]) return;

        switch (rx_buf[1]) {
        case 0x01:  /* 球 X 坐标 mm */
            ball_x_mm = (int16_t)((rx_buf[2] << 8) | rx_buf[3]);
            ball_updated = true;
            break;
        case 0x05:  /* 车速 + 轨道 */
            car_track   = rx_buf[2];
            car_speed   = rx_buf[3];
            car_updated = true;
            break;
        default: break;
        }
    }
}

/* ==================== PID 切换 + 前馈 ==================== */
static void switch_pid(void)
{
    if (car_updated) {
        car_updated = false;
        track_type = (car_track != 0) ? TRACK_CURVE : TRACK_STRAIGHT;

        if (track_type == TRACK_STRAIGHT) {
            pid.kp = (float)PID_S_KP / PID_SCALE;
            pid.ki = (float)PID_S_KI / PID_SCALE;
            pid.kd = (float)PID_S_KD / PID_SCALE;
            pid.target = 0;
        } else {
            pid.kp = (float)PID_C_KP / PID_SCALE;
            pid.ki = (float)PID_C_KI / PID_SCALE;
            pid.kd = (float)PID_C_KD / PID_SCALE;
            /* 前馈 target = v² × k, 占位, 现场标定 */
            int32_t v = (int32_t)car_speed * 10;   /* cm/s → mm/s */
            float ff = 0;
            if (v > FF_MIN_V) {
                ff = (float)(v * v) * FF_SCALE / 500000.0f;
                if (ff >  FF_TARGET_MAX) ff =  FF_TARGET_MAX;
                if (ff < -FF_TARGET_MAX) ff = -FF_TARGET_MAX;
            }
            pid.target = ff;
        }
    }
}

/* ==================== 控制循环 (每10ms, TIMG中断) ==================== */
void Stepper_Tick(void)
{
    switch_pid();

    if (!ball_updated) {
        if (fabsf(pid.output) > 0.5f) {
            pid.output *= 0.92f; pid.integral *= 0.90f;
        } else {
            Stepper_Stop(); pid.output = 0; pid.integral = 0;
        }
        return;
    }
    ball_updated = false;

    /* PID */
    pid.error = pid.target - (float)ball_x_mm;

    if (fabsf(pid.error) < 30.0f) {
        pid.integral += pid.error;
        if (pid.integral >  200.0f) pid.integral =  200.0f;
        if (pid.integral < -200.0f) pid.integral = -200.0f;
    } else {
        pid.integral *= 0.95f;
    }

    float derivative = pid.error - pid.last_error;
    pid.last_error = pid.error;
    pid.output = pid.kp * pid.error + pid.ki * pid.integral + pid.kd * derivative;

    int32_t out_max = (track_type == TRACK_STRAIGHT) ? PID_S_OUTMAX : PID_C_OUTMAX;
    if (pid.output >  (float)out_max) pid.output =  (float)out_max;
    if (pid.output < -(float)out_max) pid.output = -(float)out_max;
    pid.output_int = (int)pid.output;

    /* 电机 */
    int freq = (int)fabsf(pid.output);
    if (freq < STEP_MIN_FREQ) {
        Stepper_Stop();
    } else if (pid.output > 0) {
        Stepper_SetDir(0, 0); Stepper_SetDir(1, 0);
        Stepper_SetFreq(0, freq); Stepper_SetFreq(1, freq);
    } else {
        Stepper_SetDir(0, 1); Stepper_SetDir(1, 1);
        Stepper_SetFreq(0, freq); Stepper_SetFreq(1, freq);
    }
}

/* ==================== 电机驱动 ==================== */
void Stepper_SetDir(int motor, int dir)
{
    if (motor == 0) {
        if (dir) DL_GPIO_setPins(STEP_DIR_L_PORT, STEP_DIR_L_PIN);
        else     DL_GPIO_clearPins(STEP_DIR_L_PORT, STEP_DIR_L_PIN);
    } else {
        if (dir) DL_GPIO_setPins(STEP_DIR_R_PORT, STEP_DIR_R_PIN);
        else     DL_GPIO_clearPins(STEP_DIR_R_PORT, STEP_DIR_R_PIN);
    }
}

void Stepper_SetFreq(int motor, int hz)
{
    if (hz < 1) hz = 1;
    if (hz > STEP_MAX_FREQ) hz = STEP_MAX_FREQ;

    /* TIMG12 32MHz, period=32000, 1kHz基频, CC=16000 (50%) */
    uint32_t period = 32000000U / (uint32_t)hz;
    if (period < 10U) period = 10U;
    if (period > 65535U) period = 65535U;

    DL_TimerG_setPeriod(STEP_PWM_INST, (uint16_t)period);
    uint16_t cc_val = (uint16_t)(period / 2U);

    if (motor == 0) {
        DL_Timer_setCaptureCompareValue(STEP_PWM_INST, cc_val, STEP_PWM_L_CH);
    } else {
        DL_Timer_setCaptureCompareValue(STEP_PWM_INST, cc_val, STEP_PWM_R_CH);
    }
}

void Stepper_Stop(void)
{
    DL_Timer_setCaptureCompareValue(STEP_PWM_INST, 0, STEP_PWM_L_CH);
    DL_Timer_setCaptureCompareValue(STEP_PWM_INST, 0, STEP_PWM_R_CH);
}
