/*
 *  motor.c  --  双路直流电机驱动实现
 *
 *  PWM: 边沿对齐下行计数, 占空比 = (3200 - CC) / 3200
 *       CC=500→84%, CC=2000→37%, 共用一个 TIMG0
 */
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>
#include "motor.h"

/* ---- Motor 硬件配置 ---- */
#define PWMA_INST               TIMG0
#define GPIO_PWMA_C0_IDX        DL_TIMER_CC_0_INDEX
#define GPIO_PWMA_C1_IDX        DL_TIMER_CC_1_INDEX

#define DC_MOTOR_AIN1_PORT      GPIOA
#define DC_MOTOR_AIN1_PIN       DL_GPIO_PIN_8
#define DC_MOTOR_AIN2_PORT      GPIOA
#define DC_MOTOR_AIN2_PIN       DL_GPIO_PIN_15

#define DC_MOTOR_BIN1_PORT      GPIOB
#define DC_MOTOR_BIN1_PIN       DL_GPIO_PIN_13
#define DC_MOTOR_BIN2_PORT      GPIOB
#define DC_MOTOR_BIN2_PIN       DL_GPIO_PIN_12

#define DC_MOTOR_STBY_PORT      GPIOB
#define DC_MOTOR_STBY_PIN       DL_GPIO_PIN_24

/* 电机通道映射 */
static const struct {
    uint32_t   ain1_pin, ain2_pin;
    GPIO_Regs *ain1_port, *ain2_port;
    DL_TIMER_CC_INDEX cc_idx;
} MotorCh[2] = {
    { DC_MOTOR_AIN1_PIN, DC_MOTOR_AIN2_PIN,
      DC_MOTOR_AIN1_PORT, DC_MOTOR_AIN2_PORT,
      GPIO_PWMA_C0_IDX },                          /* A 通道 */
    { DC_MOTOR_BIN1_PIN, DC_MOTOR_BIN2_PIN,
      DC_MOTOR_BIN1_PORT, DC_MOTOR_BIN2_PORT,
      GPIO_PWMA_C1_IDX },                          /* B 通道 */
};

/* ---- 初始化 (方向引脚已在 syscfg 中配置为输出, 此处不重复) ---- */
void Motor_Init(void)
{
    Motor_SetDir(MOTOR_A, MOTOR_FWD);
    Motor_SetDir(MOTOR_B, MOTOR_FWD);
}

/* ---- 设置方向 ---- */
void Motor_SetDir(uint32_t motor, uint32_t dir)
{
    if (motor > MOTOR_B) return;

    if (dir == MOTOR_FWD) {
        DL_GPIO_clearPins(MotorCh[motor].ain1_port, MotorCh[motor].ain1_pin);
        DL_GPIO_setPins(  MotorCh[motor].ain2_port, MotorCh[motor].ain2_pin);
    } else if (dir == MOTOR_REV) {
        DL_GPIO_setPins(  MotorCh[motor].ain1_port, MotorCh[motor].ain1_pin);
        DL_GPIO_clearPins(MotorCh[motor].ain2_port, MotorCh[motor].ain2_pin);
    } else { /* MOTOR_STOP: 短路制动 */
        DL_GPIO_clearPins(MotorCh[motor].ain1_port, MotorCh[motor].ain1_pin);
        DL_GPIO_clearPins(MotorCh[motor].ain2_port, MotorCh[motor].ain2_pin);
    }
}

/* ---- 设置速度 (CC 值, 0=最快, 3200=最慢/停止) ---- */
void Motor_SetSpeed(uint32_t motor, uint32_t cc)
{
    if (motor > MOTOR_B) return;
    DL_Timer_setCaptureCompareValue(
        PWMA_INST, cc, MotorCh[motor].cc_idx);
}

/* ---- 使能驱动 + 启动 PWM ---- */
void Motor_Enable(void)
{
    DL_GPIO_setPins(DC_MOTOR_STBY_PORT, DC_MOTOR_STBY_PIN);
    DL_TimerG_startCounter(PWMA_INST);
}

/* ================================================================
 *  增量式 PI 速度控制
 *  Δu = Kp*(e[k]-e[k-1]) + Ki*e[k]
 * ================================================================ */

#define PID_KP   30     /* 比例系数 ×10, 实际Kp=3  */
#define PID_KI   50     /* 积分系数 ×10, 实际Ki=5  */
#define PID_SCALE 10    /* 定点缩放 */
#define PID_DU_MAX 80   /* 单次du上限(防过冲) */
#define PID_CC_MAX  3199
#define PID_CC_MIN  0

typedef struct {
    int32_t  target;     /* 目标速度 mm/s */
    int32_t  last_err;   /* 上次误差 */
    int32_t  cc;         /* 当前PWM比较值 (0=慢 → 3200=快) */
} PID_t;

static PID_t gPID[2] = {
    { .target = 0, .last_err = 0, .cc = 3200 },  /* 与SysConfig初始值一致 */
    { .target = 0, .last_err = 0, .cc = 3200 },
};

void Motor_SetTarget(uint32_t motor, int32_t speed_mms)
{
    if (motor > MOTOR_B) return;
    gPID[motor].target = speed_mms;
}

void Motor_PID_Update(int32_t speedA, int32_t speedB)
{
    int32_t speeds[2] = { speedA, speedB };
    uint32_t m;

    for (m = 0; m <= MOTOR_B; m++) {
        int32_t e = gPID[m].target - speeds[m];
        int32_t du = (PID_KP * (e - gPID[m].last_err)
                    + PID_KI * e) / PID_SCALE;
        gPID[m].last_err = e;

        /* 单次du限幅, 防止过冲  */
        if (du >  PID_DU_MAX) du =  PID_DU_MAX;
        if (du < -PID_DU_MAX) du = -PID_DU_MAX;

        int32_t new_cc = (int32_t)gPID[m].cc - du;    /* CC↓→占空比↑→加速 */
        if (new_cc < PID_CC_MIN) new_cc = PID_CC_MIN;
        if (new_cc > PID_CC_MAX) new_cc = PID_CC_MAX;
        gPID[m].cc = (uint32_t)new_cc;

        Motor_SetSpeed(m, gPID[m].cc);
    }
}
