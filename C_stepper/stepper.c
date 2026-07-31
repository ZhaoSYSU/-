/*
 * stepper.c  --  步进电机控球 (MSPM0G3507)
 *
 * 主控板(A_Driver) ──UART──▶ 本板
 *   CMD 0x01: ball_x_mm   CMD 0x05: track + speed_cms
 *
 * 双PID+前馈 → TIMG12 PWM → 步进电机
 */
#include "ti_msp_dl_config.h"
#include "stepper.h"
#include <math.h>

#define STEP_PWM_INST    TIMG12
#define STEP_DIR_L_PORT   GPIOA
#define STEP_DIR_L_PIN    DL_GPIO_PIN_5
#define STEP_DIR_R_PORT   GPIOA
#define STEP_DIR_R_PIN    DL_GPIO_PIN_6
#define STEP_PWM_L_CH     DL_TIMER_CC_0_INDEX
#define STEP_PWM_R_CH     DL_TIMER_CC_1_INDEX
#define CONTROL_TIMER     TIMG6
#define FRAME_HEADER      0xAAU
#define FRAME_FOOTER      0x55U

static PID_t     pid;
static TrackType track_type = TRACK_STRAIGHT;
static volatile int16_t  ball_x_mm;
static volatile bool     ball_updated;
static volatile uint8_t  car_track, car_speed;
static volatile bool     car_updated;
static uint8_t  rx_buf[6], rx_idx;
static bool     rx_synced;

void Stepper_Init(void)
{
    pid.kp = (float)PID_S_KP / PID_SCALE;
    pid.ki = (float)PID_S_KI / PID_SCALE;
    pid.kd = (float)PID_S_KD / PID_SCALE;
    pid.target = 0; pid.error = 0; pid.last_error = 0;
    pid.integral = 0; pid.output = 0; pid.output_int = 0;
    ball_x_mm = 0; ball_updated = false;
    car_track = 0; car_speed = 0; car_updated = false;
    track_type = TRACK_STRAIGHT;
    rx_idx = 0; rx_synced = false;
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
}

void Stepper_UART_RX(uint8_t byte)
{
    if (!rx_synced) {
        if (byte == FRAME_HEADER) { rx_buf[0] = byte; rx_idx = 1; rx_synced = true; }
        return;
    }
    rx_buf[rx_idx++] = byte;
    if (rx_idx >= 6) {
        rx_synced = false;
        if (rx_buf[5] != FRAME_FOOTER) return;
        if (((rx_buf[1]+rx_buf[2]+rx_buf[3])&0xFFU) != rx_buf[4]) return;
        switch (rx_buf[1]) {
        case 0x01: ball_x_mm = (int16_t)((rx_buf[2]<<8)|rx_buf[3]); ball_updated = true; break;
        case 0x05: car_track = rx_buf[2]; car_speed = rx_buf[3]; car_updated = true; break;
        }
    }
}

static void switch_pid(void)
{
    if (car_updated) {
        car_updated = false;
        track_type = (car_track != 0) ? TRACK_CURVE : TRACK_STRAIGHT;
        if (track_type == TRACK_STRAIGHT) {
            pid.kp = (float)PID_S_KP/PID_SCALE;
            pid.ki = (float)PID_S_KI/PID_SCALE;
            pid.kd = (float)PID_S_KD/PID_SCALE;
            pid.target = 0;
        } else {
            pid.kp = (float)PID_C_KP/PID_SCALE;
            pid.ki = (float)PID_C_KI/PID_SCALE;
            pid.kd = (float)PID_C_KD/PID_SCALE;
            int32_t v = (int32_t)car_speed*10;
            float ff = 0;
            if (v > FF_MIN_V) {
                ff = (float)(v*v)*FF_SCALE/500000.0f;
                if (ff >  FF_TARGET_MAX) ff =  FF_TARGET_MAX;
                if (ff < -FF_TARGET_MAX) ff = -FF_TARGET_MAX;
            }
            pid.target = ff;
        }
    }
}

void Stepper_Tick(void)
{
    switch_pid();
    if (!ball_updated) {
        if (fabsf(pid.output) > 0.5f) { pid.output *= 0.92f; pid.integral *= 0.90f; }
        else { Stepper_Stop(); pid.output = 0; pid.integral = 0; }
        return;
    }
    ball_updated = false;
    pid.error = pid.target - (float)ball_x_mm;
    if (fabsf(pid.error) < 30.0f) {
        pid.integral += pid.error;
        if (pid.integral >  200.0f) pid.integral =  200.0f;
        if (pid.integral < -200.0f) pid.integral = -200.0f;
    } else { pid.integral *= 0.95f; }
    float derivative = pid.error - pid.last_error;
    pid.last_error = pid.error;
    pid.output = pid.kp*pid.error + pid.ki*pid.integral + pid.kd*derivative;
    int32_t omax = (track_type==TRACK_STRAIGHT) ? PID_S_OUTMAX : PID_C_OUTMAX;
    if (pid.output >  (float)omax) pid.output =  (float)omax;
    if (pid.output < -(float)omax) pid.output = -(float)omax;
    pid.output_int = (int)pid.output;

    int freq = (int)fabsf(pid.output);
    if (freq < STEP_MIN_FREQ) { Stepper_Stop(); }
    else if (pid.output > 0) {
        Stepper_SetDir(0,0); Stepper_SetDir(1,0);
        Stepper_SetFreq(0,freq); Stepper_SetFreq(1,freq);
    } else {
        Stepper_SetDir(0,1); Stepper_SetDir(1,1);
        Stepper_SetFreq(0,freq); Stepper_SetFreq(1,freq);
    }
}

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
    if (hz < 1) hz = 1; if (hz > STEP_MAX_FREQ) hz = STEP_MAX_FREQ;
    uint32_t period = 32000000U / (uint32_t)hz;
    if (period < 10U) period = 10U; if (period > 65535U) period = 65535U;
    DL_TimerG_setPeriod(STEP_PWM_INST, (uint16_t)period);
    uint16_t cc = (uint16_t)(period/2U);
    if (motor==0) DL_Timer_setCaptureCompareValue(STEP_PWM_INST, cc, STEP_PWM_L_CH);
    else          DL_Timer_setCaptureCompareValue(STEP_PWM_INST, cc, STEP_PWM_R_CH);
}

void Stepper_Stop(void)
{
    DL_Timer_setCaptureCompareValue(STEP_PWM_INST, 0, STEP_PWM_L_CH);
    DL_Timer_setCaptureCompareValue(STEP_PWM_INST, 0, STEP_PWM_R_CH);
}
