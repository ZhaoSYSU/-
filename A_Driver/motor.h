/*
 *  motor.h  --  双路直流电机驱动 (TB6612FNG)
 *
 *  A通道: AIN1=PA8, AIN2=PA15, PWMA=PA12 (TIMG0 CCP0)
 *  B通道: BIN1=PB13, BIN2=PB12, PWMB=PA13 (TIMG0 CCP1)
 *  STBY:  PB24
 */
#ifndef MOTOR_H
#define MOTOR_H

/* 电机编号 */
#define MOTOR_A  0
#define MOTOR_B  1

/* 方向 */
#define MOTOR_FWD   1   /* 正转 */
#define MOTOR_REV   0   /* 反转 */
#define MOTOR_STOP  2   /* 停止 (短路制动) */

void Motor_Init(void);
void Motor_SetDir(uint32_t motor, uint32_t dir);
void Motor_SetSpeed(uint32_t motor, uint32_t cc);
void Motor_Enable(void);

/* ---- PID 速度控制 ---- */
void Motor_SetTarget(uint32_t motor, int32_t speed_mms);
void Motor_PID_Update(int32_t speedA, int32_t speedB);

#endif
