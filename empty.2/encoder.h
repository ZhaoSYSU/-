/*
 *  encoder.h  --  双路编码器读取
 *
 *  电机A: E1A=PA17, E1B=PA18
 *  电机B: E2A=PB4,  E2B=PB15
 */
#ifndef ENCODER_H
#define ENCODER_H

void Encoder_Init(void);
int32_t Encoder_GetSpeed_A(void);   /* mm/s */
int32_t Encoder_GetSpeed_B(void);   /* mm/s */
int32_t Encoder_GetPulse_A(void);  /* 原始脉冲(调试) */
int32_t Encoder_GetPulse_B(void);

#endif
