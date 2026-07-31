/*
 * control.c  --  步进电机控球 (STM32F103RCT6 + ATD5984)
 *
 * 主控板(MSPM0) ──UART──▶ 本板(STM32)
 *   接收 CMD 0x01=球X坐标,  CMD 0x05=车速+轨道类型
 *   双PID+前馈 → TIM8 PWM → 步进电机
 */
#include "control.h"
#include "ATD5984.h"
#include <math.h>

PID_t     pid;
TrackType track_type = TRACK_STRAIGHT;

static volatile int16_t  ball_x_mm;
static volatile bool     ball_updated;
static volatile uint8_t  car_track;
static volatile uint8_t  car_speed;
static volatile bool     car_updated;

static uint8_t  rx_buf[6];
static uint8_t  rx_idx;
static bool     rx_synced;

void Control_Init(void)
{
    pid.kp = (float)PID_S_KP / PID_SCALE;
    pid.ki = (float)PID_S_KI / PID_SCALE;
    pid.kd = (float)PID_S_KD / PID_SCALE;
    pid.target = 0;
    pid.error = 0; pid.last_error = 0; pid.integral = 0;
    pid.output = 0; pid.output_int = 0;

    ball_x_mm = 0; ball_updated = false;
    car_track = 0; car_speed = 0; car_updated = false;
    track_type = TRACK_STRAIGHT;
    rx_idx = 0; rx_synced = false;
}

/* ---- UART 接收 (USART1中断, 从主控板来) ---- */
void UART_RX_Handler(uint8_t byte)
{
    if (!rx_synced) {
        if (byte == UART_HEADER) { rx_buf[0] = byte; rx_idx = 1; rx_synced = true; }
        return;
    }
    rx_buf[rx_idx++] = byte;
    if (rx_idx >= 6) {
        rx_synced = false;
        if (rx_buf[5] != UART_FOOTER) return;
        if (((rx_buf[1] + rx_buf[2] + rx_buf[3]) & 0xFF) != rx_buf[4]) return;
        switch (rx_buf[1]) {
        case 0x01: ball_x_mm = (int16_t)((rx_buf[2]<<8)|rx_buf[3]); ball_updated = true; break;
        case 0x05: car_track = rx_buf[2]; car_speed = rx_buf[3]; car_updated = true; break;
        }
    }
}

/* ---- PID 切换 ---- */
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
            int32_t v = (int32_t)car_speed * 10;
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

/* ---- 控制循环 (TIM2中断, 每10ms) ---- */
void Control_Tick(void)
{
    switch_pid();
    if (!ball_updated) {
        if (fabsf(pid.output) > 0.5f) { pid.output *= 0.92f; pid.integral *= 0.90f; }
        else { Motor_Stop(); pid.output = 0; pid.integral = 0; }
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
    pid.output = pid.kp * pid.error + pid.ki * pid.integral + pid.kd * derivative;

    int32_t omax = (track_type == TRACK_STRAIGHT) ? PID_S_OUTMAX : PID_C_OUTMAX;
    if (pid.output >  (float)omax) pid.output =  (float)omax;
    if (pid.output < -(float)omax) pid.output = -(float)omax;
    pid.output_int = (int)pid.output;

    int freq = (int)fabsf(pid.output);
    if (freq < STEP_MIN_FREQ) { Motor_Stop(); }
    else if (pid.output > 0) {
        Motor_SetDir(0,0); Motor_SetDir(1,0);
        Motor_SetFreq(0,freq); Motor_SetFreq(1,freq);
    } else {
        Motor_SetDir(0,1); Motor_SetDir(1,1);
        Motor_SetFreq(0,freq); Motor_SetFreq(1,freq);
    }
}

/* ---- 电机驱动 (TIM8, 同 ATD5984 参考工程) ---- */
void Motor_SetDir(int motor, int dir)
{
    if (motor == 0) {
        if (dir) GPIO_SetBits(GPIOC, GPIO_Pin_13);
        else     GPIO_ResetBits(GPIOC, GPIO_Pin_13);
    } else {
        if (dir) GPIO_SetBits(GPIOC, GPIO_Pin_14);
        else     GPIO_ResetBits(GPIOC, GPIO_Pin_14);
    }
}

void Motor_SetFreq(int motor, int hz)
{
    if (hz < 1) hz = 1; if (hz > STEP_MAX_FREQ) hz = STEP_MAX_FREQ;
    uint32_t tim_clk = 32000000 / (STEP_TIM_PSC + 1);
    uint32_t new_arr = (tim_clk / (hz * 2)) - 1;
    if (new_arr < 10) new_arr = 10;
    if (new_arr > 65535) new_arr = 65535;
    TIM_SetAutoreload(TIM8, (uint16_t)new_arr);
    TIM_SetCompare3(TIM8, (uint16_t)(new_arr/2));
    TIM_SetCompare4(TIM8, (uint16_t)(new_arr/2));
}

void Motor_Stop(void)
{
    TIM_SetCompare3(TIM8, 0);
    TIM_SetCompare4(TIM8, 0);
}
