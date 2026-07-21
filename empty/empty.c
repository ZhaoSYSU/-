/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"

/* ========== SysTick 用作 PID 定时器 (CPUCLK=32MHz, 100ms=3.2M ticks) ========== */
#define SYSTICK_PERIOD  3199999U

/* ========== 编码器脉冲计数 ========== */
static volatile int32_t g_enc1_pulses;  /* 右轮(从) */
static volatile int32_t g_enc2_pulses;  /* 左轮(主) */

/* ========== 简易比例速度匹配 (代替浮点PID, 避免截断问题) ========== */
#define PWM_PERIOD   199
#define MASTER_PWM   40        /* 左轮(主)固定占空比, 改这里调速 */
#define KP_STEP      5         /* 每100ms脉冲差1=修正5步 */

/* ========== 毫秒延时 ========== */
static void delay_ms(uint32_t ms)
{
    delay_cycles((CPUCLK_FREQ / 1000) * ms);
}

/* ========== 编码器引脚边沿检测 ========== */
static uint8_t g_enc1a_last, g_enc2a_last;

static inline void encoder_poll(void)
{
    uint8_t a1 = (DL_GPIO_readPins(GPIOA, DL_GPIO_PIN_17) != 0) ? 1 : 0;
    uint8_t a2 = (DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_6)  != 0) ? 1 : 0;
    if (a1 != g_enc1a_last) { g_enc1_pulses++; g_enc1a_last = a1; }
    if (a2 != g_enc2a_last) { g_enc2_pulses++; g_enc2a_last = a2; }
}

/* ========== 主函数 ========== */
int main(void)
{
    SYSCFG_DL_init();

    /* ---- 初始化 ---- */
    g_enc1a_last = g_enc2a_last = 0;
    int32_t slave_pwm = MASTER_PWM;  /* 右轮从与左轮相同的PWM起步 */

    /* ---- 初始化 SysTick (100ms) ---- */
    SysTick->LOAD = SYSTICK_PERIOD;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    /* ---- 就绪信号 ---- */
    DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN1_PIN);
    delay_ms(100);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN1_PIN);

    /* ---- 等待 S2 ---- */
    while (DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_21) != 0) { encoder_poll(); }
    delay_ms(50);
    while (DL_GPIO_readPins(GPIOB, DL_GPIO_PIN_21) == 0) {}
    delay_ms(50);

    /* ---- 正转 ---- */
    DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN2_PIN);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_AIN1_PIN);
    DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN2_PIN);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_BIN1_PIN);

    /* ---- 启动 PWM ---- */
    DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST, MASTER_PWM, DL_TIMER_CC_1_INDEX);
    DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST, slave_pwm, DL_TIMER_CC_0_INDEX);
    DL_TimerG_startCounter(PWM_MOTOR_INST);

    /* ---- 控制循环 5 秒 (50 × 100ms) ---- */
    for (uint32_t tick = 0; tick < 50; tick++)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) {
            encoder_poll();
        }

        /* 读脉冲: enc2=左轮(主), enc1=右轮(从) */
        int32_t m = g_enc2_pulses; g_enc2_pulses = 0;
        int32_t s = g_enc1_pulses; g_enc1_pulses = 0;

        /* 增量式修正: 从轮慢了加, 快了减 */
        if (m > s) {
            slave_pwm += KP_STEP;
        } else if (m < s) {
            slave_pwm -= KP_STEP;
        }
        if (slave_pwm > PWM_PERIOD) slave_pwm = PWM_PERIOD;
        if (slave_pwm < 5)          slave_pwm = 5;

        DL_TimerG_setCaptureCompareValue(
            PWM_MOTOR_INST, (uint16_t)slave_pwm, DL_TIMER_CC_0_INDEX);
    }

    /* ---- 停止 ---- */
    SysTick->CTRL = 0;
    DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST, 0, DL_TIMER_CC_0_INDEX);
    DL_TimerG_setCaptureCompareValue(PWM_MOTOR_INST, 0, DL_TIMER_CC_1_INDEX);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT,
        GPIO_MOTOR_AIN1_PIN | GPIO_MOTOR_AIN2_PIN |
        GPIO_MOTOR_BIN1_PIN | GPIO_MOTOR_BIN2_PIN);

    while (1) { __WFI(); }
}
