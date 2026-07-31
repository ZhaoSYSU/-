/*
 * stepper.h  --  步进电机控球 (MSPM0G3507)
 *
 * 主控板(A_Driver) ──UART──▶ 本板
 *   CMD 0x01: ball_x_mm (int16)   CMD 0x05: track + speed
 *
 * 双PID+前馈 → TIMG PWM → 步进电机 → 摆杆
 * PID参数为占位值, 待现场标定
 */
#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

/* ============ 直道 PID (占位) ============ */
#define PID_S_KP      1000    /* ×1000, 实际1.0 */
#define PID_S_KI      0
#define PID_S_KD      0
#define PID_S_OUTMAX  6000

/* ============ 弯道 PID (占位) ============ */
#define PID_C_KP      1000
#define PID_C_KI      0
#define PID_C_KD      0
#define PID_C_OUTMAX  7000

/* ============ 前馈 (占位) ============ */
#define FF_MIN_V      30
#define FF_SCALE      100
#define FF_TARGET_MAX 25

#define PID_SCALE      1000
#define STEP_MAX_FREQ  8000
#define STEP_MIN_FREQ  200

typedef enum { TRACK_STRAIGHT = 0, TRACK_CURVE = 1 } TrackType;

typedef struct {
    float kp, ki, kd;
    float target, error, last_error, integral;
    float output;
    int   output_int;
} PID_t;

void Stepper_Init(void);
void Stepper_Tick(void);              /* 10ms, TIMG中断 */
void Stepper_UART_RX(uint8_t byte);   /* UART中断 */
void Stepper_SetDir(int motor, int dir);
void Stepper_SetFreq(int motor, int hz);
void Stepper_Stop(void);

#endif
