/*
 * stepper.h  --  步进电机控球 (MSPM0G3507)
 *
 * 主控板 ──UART──▶ 本板
 *   接收 CMD 0x01=球X坐标, CMD 0x05=车速+轨道类型
 *   双PID+前馈 → TIMG PWM → 步进电机
 *
 * PID参数: 待现场标定, 当前为占位值
 */
#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

/* ============ 直道 PID (占位值, 现场调) ============ */
#define PID_S_KP      1000    /* 比例 ×1000, 实际1.0 */
#define PID_S_KI      0       /* 积分, 先不启用 */
#define PID_S_KD      0       /* 微分, 先不启用 */
#define PID_S_OUTMAX  6000    /* 输出限幅 (步进频率 Hz) */

/* ============ 弯道 PID + 前馈 (占位值, 现场调) ============ */
#define PID_C_KP      1000
#define PID_C_KI      0
#define PID_C_KD      0
#define PID_C_OUTMAX  7000

/* ============ 前馈 (占位值) ============ */
#define FF_MIN_V      30      /* 最低触发车速 mm/s */
#define FF_SCALE      100     /* 定点缩放 */
#define FF_TARGET_MAX 25      /* 前馈最大偏移 mm */

/* ============ 通用 ============ */
#define PID_SCALE      1000
#define STEP_MAX_FREQ  8000
#define STEP_MIN_FREQ  200

/* 轨道类型 */
typedef enum { TRACK_STRAIGHT = 0, TRACK_CURVE = 1 } TrackType;

/* PID 状态 */
typedef struct {
    float kp, ki, kd;
    float target;
    float error, last_error, integral;
    float output;
    int   output_int;
} PID_t;

/* ============ API ============ */
void Stepper_Init(void);
void Stepper_Tick(void);                 /* 10ms, TIMG中断调用 */
void Stepper_UART_RX(uint8_t byte);      /* UART中断喂字节 */

void Stepper_SetDir(int motor, int dir); /* 0=左 1=右, 0=正 1=反 */
void Stepper_SetFreq(int motor, int hz);
void Stepper_Stop(void);

#endif
