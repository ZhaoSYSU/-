/*
 * ================================================================
 *  LP_MSPM0G3507 双电机机器人平台 — 引脚接线总表
 * ================================================================
 *
 *  【电机A — TB6612FNG 通道1】
 *    PA8   (PINCM19, pin15)  → AIN1   (方向控制)
 *    PA15  (PINCM37, pin33)  → AIN2   (方向控制)
 *    PA12  (PINCM34, pin27)  → PWMA   (TIMG0 CCP0, 边沿对齐下行)
 *
 *  【电机B — TB6612FNG 通道2】
 *    PB13  (PINCM30, pin51)  → BIN1   (方向控制)
 *    PB12  (PINCM29, pin50)  → BIN2   (方向控制)
 *    PA13  (PINCM35, pin29)  → PWMB   (TIMG0 CCP1, 边沿对齐下行)
 *
 *  【TB6612FNG 公共】
 *    PB24  (PINCM52, pin61)  → STBY   (待机, 高电平使能)
 *
 *  【编码器A — MG310 电机1 (A相双沿计数, 2倍频, B判方向)】
 *    PA17  (PINCM39, pin37)  → E1A    (编码器A相, 输入+上拉+迟滞)
 *    PA18  (PINCM40, pin38)  → E1B    (编码器B相, 输入+上拉+迟滞)
 *
 *  【编码器B — MG310 电机2】
 *    PB4   (PINCM17, pin43)  → E2A    (编码器A相, 输入+上拉+迟滞)
 *    PB15  (PINCM32, pin54)  → E2B    (编码器B相, 输入+上拉+迟滞)
 *
 *  【OLED — SSD1306 128×64 SPI (7-pin)】
 *    PB9   (PINCM18, pin46)  → SCLK   (SPI1 时钟)
 *    PB8   (PINCM55, pin45)  → MOSI   (SPI1 数据)
 *    PB3   (PINCM16, pin42)  → RES    (复位, GPIO)
 *    PB2   (PINCM15, pin41)  → DC     (数据/命令, GPIO)
 *    PA27  (PINCM54, pin36)  → CS     (片选, GPIO)
 *    VCC → 3.3V,  GND → GND
 *
 *  【调试接口 (板载 XDS-110)】
 *    PA20  → SWCLK,  PA19  → SWDIO
 *
 *  【定时器分配】
 *    TIMG0  → PWM 电机A+B, 32MHz, period=3200, ~10kHz
 *    TIMG12 → 编码器轮询, 32MHz, period=32000, 1ms 周期
 *    TIMG6  → 速度计算+PID, BUSCLK/256=125kHz, period=12500, 100ms
 *
 *  【编码参数】
 *    电机编码器: 20 PPR, 减速比 13:1
 *    轮子直径: 48mm, 周长 150.72mm
 *    CPR(2倍频): 20×13×2 = 520 脉冲/轮转
 *    速度公式: mm/s = pulse_count × 942 ÷ 325
 *
 * ================================================================
 *
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
#include "oled.h"
#include "motor.h"
#include "encoder.h"

int main(void)
{
    SYSCFG_DL_init();
    Encoder_Init();

    /* OLED 显示开机信息 */
    OLED_Init();
    OLED_Clear();
    OLED_Refresh();

    /* 双电机: 正转 + 设目标速度 */
    Motor_Init();
    Motor_SetTarget(MOTOR_A, 30);
    Motor_SetTarget(MOTOR_B, 30);
    Motor_Enable();

    while (1) {
        OLED_Clear();
        {
            char buf[16];
            int32_t spdA = Encoder_GetSpeed_A();
            int32_t spdB = Encoder_GetSpeed_B();
            int32_t pulA = Encoder_GetPulse_A();
            int32_t pulB = Encoder_GetPulse_B();

            /* 第一行: 速度 mm/s */
            OLED_ShowString(1, 0, "V A:");
            {
                int i = 0; int32_t v = spdA;
                if (v < 0) { buf[i++] = '-'; v = -v; }
                buf[i++] = '0' + ((v / 10000) % 10);
                buf[i++] = '0' + ((v / 1000)  % 10);
                buf[i++] = '0' + ((v / 100)   % 10);
                buf[i++] = '0' + ((v / 10)     % 10);
                buf[i++] = '0' + (v % 10);
                buf[i]   = 0;
                OLED_ShowString(1, 40, buf);
            }

            /* 第二行: 速度 mm/s */
            OLED_ShowString(2, 0, "V B:");
            {
                int i = 0; int32_t v = spdB;
                if (v < 0) { buf[i++] = '-'; v = -v; }
                buf[i++] = '0' + ((v / 10000) % 10);
                buf[i++] = '0' + ((v / 1000)  % 10);
                buf[i++] = '0' + ((v / 100)   % 10);
                buf[i++] = '0' + ((v / 10)     % 10);
                buf[i++] = '0' + (v % 10);
                buf[i]   = 0;
                OLED_ShowString(2, 40, buf);
            }

            /* 第三行: 原始脉冲数(调试) */
            {
                char pbuf[20];
                int i, n;
                int32_t p;

                i = 0;
                pbuf[i++] = 'P'; pbuf[i++] = 'A'; pbuf[i++] = ':';
                p = pulA;
                if (p < 0) { pbuf[i++] = '-'; p = -p; }
                if (p > 9999) p = 9999;
                n = p;
                pbuf[i++] = '0' + (n / 1000); n %= 1000;
                pbuf[i++] = '0' + (n / 100);  n %= 100;
                pbuf[i++] = '0' + (n / 10);   n %= 10;
                pbuf[i++] = '0' + n;

                pbuf[i++] = ' '; pbuf[i++] = 'P'; pbuf[i++] = 'B'; pbuf[i++] = ':';
                p = pulB;
                if (p < 0) { pbuf[i++] = '-'; p = -p; }
                if (p > 9999) p = 9999;
                n = p;
                pbuf[i++] = '0' + (n / 1000); n %= 1000;
                pbuf[i++] = '0' + (n / 100);  n %= 100;
                pbuf[i++] = '0' + (n / 10);   n %= 10;
                pbuf[i++] = '0' + n;
                pbuf[i] = 0;
                OLED_ShowString(3, 0, pbuf);
            }
        }
        OLED_Refresh();
        delay_cycles(3200000);
    }
}
