/*
 * control.h  --  步进电机控球 (STM32F103RCT6 + ATD5984)
 *
 * 主控板(MSPM0) ──UART──▶ 本板(STM32)
 *   CMD 0x01: ball_x_mm (int16)   CMD 0x05: track_type + speed_cms
 *
 * 本板: 双PID+前馈 → TIM8 PWM → 步进电机 → 摆杆
 * PID参数为占位值, 待现场标定
 */
#ifndef CONTROL_H
#define CONTROL_H

#include "sys.h"
#include <stdbool.h>

/* ============ 直道 PID (占位) ============ */
#define PID_S_KP      120     /* ×100, 实际1.2 */
#define PID_S_KI      15
#define PID_S_KD      80
#define PID_S_OUTMAX  6000

/* ============ 弯道 PID (占位) ============ */
#define PID_C_KP      100
#define PID_C_KI      20
#define PID_C_KD      100
#define PID_C_OUTMAX  7000

/* ============ 前馈 (占位) ============ */
#define FF_MIN_V      30
#define FF_SCALE      100
#define FF_TARGET_MAX 25

#define PID_SCALE      100
#define STEP_MAX_FREQ  8000
#define STEP_MIN_FREQ  200
#define STEP_TIM_PSC   6
#define UART_HEADER    0xAA
#define UART_FOOTER    0x55

typedef enum { TRACK_STRAIGHT = 0, TRACK_CURVE = 1 } TrackType;

typedef struct {
    float kp, ki, kd;
    float target, error, last_error, integral;
    float output;
    int   output_int;
} PID_t;

extern PID_t     pid;
extern TrackType track_type;

void Control_Init(void);
void Control_Tick(void);              /* TIM2中断, 每10ms */
void UART_RX_Handler(uint8_t byte);   /* USART1中断 */
void Motor_SetDir(int motor, int dir);
void Motor_SetFreq(int motor, int hz);
void Motor_Stop(void);

#endif
