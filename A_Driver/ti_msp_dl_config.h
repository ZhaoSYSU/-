/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
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

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)



#define CPUCLK_FREQ                                                     32000000



/* Defines for MOTOR_PWM */
#define MOTOR_PWM_INST                                                    TIMG12
#define MOTOR_PWM_INST_IRQHandler                              TIMG12_IRQHandler
#define MOTOR_PWM_INST_INT_IRQN                                (TIMG12_INT_IRQn)
#define MOTOR_PWM_INST_CLK_FREQ                                         32000000
/* GPIO defines for channel 0 */
#define GPIO_MOTOR_PWM_C0_PORT                                             GPIOB
#define GPIO_MOTOR_PWM_C0_PIN                                     DL_GPIO_PIN_13
#define GPIO_MOTOR_PWM_C0_IOMUX                                  (IOMUX_PINCM30)
#define GPIO_MOTOR_PWM_C0_IOMUX_FUNC                IOMUX_PINCM30_PF_TIMG12_CCP0
#define GPIO_MOTOR_PWM_C0_IDX                                DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_MOTOR_PWM_C1_PORT                                             GPIOA
#define GPIO_MOTOR_PWM_C1_PIN                                     DL_GPIO_PIN_31
#define GPIO_MOTOR_PWM_C1_IOMUX                                   (IOMUX_PINCM6)
#define GPIO_MOTOR_PWM_C1_IOMUX_FUNC                 IOMUX_PINCM6_PF_TIMG12_CCP1
#define GPIO_MOTOR_PWM_C1_IDX                                DL_TIMER_CC_1_INDEX



/* Defines for UART_0 */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           32000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                        DL_GPIO_PIN_11
#define GPIO_UART_0_TX_PIN                                        DL_GPIO_PIN_10
#define GPIO_UART_0_IOMUX_RX                                     (IOMUX_PINCM22)
#define GPIO_UART_0_IOMUX_TX                                     (IOMUX_PINCM21)
#define GPIO_UART_0_IOMUX_RX_FUNC                      IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                      IOMUX_PINCM21_PF_UART0_TX
#define UART_0_BAUD_RATE                                                (115200)
#define UART_0_IBRD_32_MHZ_115200_BAUD                                      (17)
#define UART_0_FBRD_32_MHZ_115200_BAUD                                      (23)





/* Defines for AIN1: GPIOB.0 with pinCMx 12 on package pin 47 */
#define MOTOR_OLED_GPIO_AIN1_PORT                                        (GPIOB)
#define MOTOR_OLED_GPIO_AIN1_PIN                                 (DL_GPIO_PIN_0)
#define MOTOR_OLED_GPIO_AIN1_IOMUX                               (IOMUX_PINCM12)
/* Defines for AIN2: GPIOA.15 with pinCMx 37 on package pin 8 */
#define MOTOR_OLED_GPIO_AIN2_PORT                                        (GPIOA)
#define MOTOR_OLED_GPIO_AIN2_PIN                                (DL_GPIO_PIN_15)
#define MOTOR_OLED_GPIO_AIN2_IOMUX                               (IOMUX_PINCM37)
/* Defines for BIN1: GPIOB.18 with pinCMx 44 on package pin 15 */
#define MOTOR_OLED_GPIO_BIN1_PORT                                        (GPIOB)
#define MOTOR_OLED_GPIO_BIN1_PIN                                (DL_GPIO_PIN_18)
#define MOTOR_OLED_GPIO_BIN1_IOMUX                               (IOMUX_PINCM44)
/* Defines for BIN2: GPIOB.19 with pinCMx 45 on package pin 16 */
#define MOTOR_OLED_GPIO_BIN2_PORT                                        (GPIOB)
#define MOTOR_OLED_GPIO_BIN2_PIN                                (DL_GPIO_PIN_19)
#define MOTOR_OLED_GPIO_BIN2_IOMUX                               (IOMUX_PINCM45)
/* Defines for STBY: GPIOA.28 with pinCMx 3 on package pin 35 */
#define MOTOR_OLED_GPIO_STBY_PORT                                        (GPIOA)
#define MOTOR_OLED_GPIO_STBY_PIN                                (DL_GPIO_PIN_28)
#define MOTOR_OLED_GPIO_STBY_IOMUX                                (IOMUX_PINCM3)
/* Defines for OLED_SCLK: GPIOB.9 with pinCMx 26 on package pin 61 */
#define MOTOR_OLED_GPIO_OLED_SCLK_PORT                                   (GPIOB)
#define MOTOR_OLED_GPIO_OLED_SCLK_PIN                            (DL_GPIO_PIN_9)
#define MOTOR_OLED_GPIO_OLED_SCLK_IOMUX                          (IOMUX_PINCM26)
/* Defines for OLED_MOSI: GPIOB.8 with pinCMx 25 on package pin 60 */
#define MOTOR_OLED_GPIO_OLED_MOSI_PORT                                   (GPIOB)
#define MOTOR_OLED_GPIO_OLED_MOSI_PIN                            (DL_GPIO_PIN_8)
#define MOTOR_OLED_GPIO_OLED_MOSI_IOMUX                          (IOMUX_PINCM25)
/* Defines for OLED_CS: GPIOB.17 with pinCMx 43 on package pin 14 */
#define MOTOR_OLED_GPIO_OLED_CS_PORT                                     (GPIOB)
#define MOTOR_OLED_GPIO_OLED_CS_PIN                             (DL_GPIO_PIN_17)
#define MOTOR_OLED_GPIO_OLED_CS_IOMUX                            (IOMUX_PINCM43)
/* Defines for OLED_DC: GPIOA.12 with pinCMx 34 on package pin 5 */
#define MOTOR_OLED_GPIO_OLED_DC_PORT                                     (GPIOA)
#define MOTOR_OLED_GPIO_OLED_DC_PIN                             (DL_GPIO_PIN_12)
#define MOTOR_OLED_GPIO_OLED_DC_IOMUX                            (IOMUX_PINCM34)
/* Defines for OLED_RST: GPIOA.13 with pinCMx 35 on package pin 6 */
#define MOTOR_OLED_GPIO_OLED_RST_PORT                                    (GPIOA)
#define MOTOR_OLED_GPIO_OLED_RST_PIN                            (DL_GPIO_PIN_13)
#define MOTOR_OLED_GPIO_OLED_RST_IOMUX                           (IOMUX_PINCM35)
/* Port definition for Pin Group ENCODER_GPIO */
#define ENCODER_GPIO_PORT                                                (GPIOB)

/* Defines for LEFT_A: GPIOB.1 with pinCMx 13 on package pin 48 */
// pins affected by this interrupt request:["LEFT_A","LEFT_B","RIGHT_A","RIGHT_B"]
#define ENCODER_GPIO_INT_IRQN                                   (GPIOB_INT_IRQn)
#define ENCODER_GPIO_INT_IIDX                   (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define ENCODER_GPIO_LEFT_A_IIDX                             (DL_GPIO_IIDX_DIO1)
#define ENCODER_GPIO_LEFT_A_PIN                                  (DL_GPIO_PIN_1)
#define ENCODER_GPIO_LEFT_A_IOMUX                                (IOMUX_PINCM13)
/* Defines for LEFT_B: GPIOB.4 with pinCMx 17 on package pin 52 */
#define ENCODER_GPIO_LEFT_B_IIDX                             (DL_GPIO_IIDX_DIO4)
#define ENCODER_GPIO_LEFT_B_PIN                                  (DL_GPIO_PIN_4)
#define ENCODER_GPIO_LEFT_B_IOMUX                                (IOMUX_PINCM17)
/* Defines for RIGHT_A: GPIOB.6 with pinCMx 23 on package pin 58 */
#define ENCODER_GPIO_RIGHT_A_IIDX                            (DL_GPIO_IIDX_DIO6)
#define ENCODER_GPIO_RIGHT_A_PIN                                 (DL_GPIO_PIN_6)
#define ENCODER_GPIO_RIGHT_A_IOMUX                               (IOMUX_PINCM23)
/* Defines for RIGHT_B: GPIOB.7 with pinCMx 24 on package pin 59 */
#define ENCODER_GPIO_RIGHT_B_IIDX                            (DL_GPIO_IIDX_DIO7)
#define ENCODER_GPIO_RIGHT_B_PIN                                 (DL_GPIO_PIN_7)
#define ENCODER_GPIO_RIGHT_B_IOMUX                               (IOMUX_PINCM24)
/* Defines for TRACK_1: GPIOA.17 with pinCMx 39 on package pin 10 */
#define TRACK_GPIO_TRACK_1_PORT                                          (GPIOA)
#define TRACK_GPIO_TRACK_1_PIN                                  (DL_GPIO_PIN_17)
#define TRACK_GPIO_TRACK_1_IOMUX                                 (IOMUX_PINCM39)
/* Defines for TRACK_2: GPIOA.18 with pinCMx 40 on package pin 11 */
#define TRACK_GPIO_TRACK_2_PORT                                          (GPIOA)
#define TRACK_GPIO_TRACK_2_PIN                                  (DL_GPIO_PIN_18)
#define TRACK_GPIO_TRACK_2_IOMUX                                 (IOMUX_PINCM40)
/* Defines for TRACK_3: GPIOB.2 with pinCMx 15 on package pin 50 */
#define TRACK_GPIO_TRACK_3_PORT                                          (GPIOB)
#define TRACK_GPIO_TRACK_3_PIN                                   (DL_GPIO_PIN_2)
#define TRACK_GPIO_TRACK_3_IOMUX                                 (IOMUX_PINCM15)
/* Defines for TRACK_4: GPIOB.3 with pinCMx 16 on package pin 51 */
#define TRACK_GPIO_TRACK_4_PORT                                          (GPIOB)
#define TRACK_GPIO_TRACK_4_PIN                                   (DL_GPIO_PIN_3)
#define TRACK_GPIO_TRACK_4_IOMUX                                 (IOMUX_PINCM16)
/* Defines for TRACK_5: GPIOB.12 with pinCMx 29 on package pin 64 */
#define TRACK_GPIO_TRACK_5_PORT                                          (GPIOB)
#define TRACK_GPIO_TRACK_5_PIN                                  (DL_GPIO_PIN_12)
#define TRACK_GPIO_TRACK_5_IOMUX                                 (IOMUX_PINCM29)
/* Defines for TRACK_6: GPIOB.15 with pinCMx 32 on package pin 3 */
#define TRACK_GPIO_TRACK_6_PORT                                          (GPIOB)
#define TRACK_GPIO_TRACK_6_PIN                                  (DL_GPIO_PIN_15)
#define TRACK_GPIO_TRACK_6_IOMUX                                 (IOMUX_PINCM32)
/* Defines for TRACK_7: GPIOB.16 with pinCMx 33 on package pin 4 */
#define TRACK_GPIO_TRACK_7_PORT                                          (GPIOB)
#define TRACK_GPIO_TRACK_7_PIN                                  (DL_GPIO_PIN_16)
#define TRACK_GPIO_TRACK_7_IOMUX                                 (IOMUX_PINCM33)
/* Defines for TRACK_8: GPIOA.7 with pinCMx 14 on package pin 49 */
#define TRACK_GPIO_TRACK_8_PORT                                          (GPIOA)
#define TRACK_GPIO_TRACK_8_PIN                                   (DL_GPIO_PIN_7)
#define TRACK_GPIO_TRACK_8_IOMUX                                 (IOMUX_PINCM14)
/* Port definition for Pin Group START_BUTTON_GPIO */
#define START_BUTTON_GPIO_PORT                                           (GPIOB)

/* Defines for S2: GPIOB.21 with pinCMx 49 on package pin 20 */
#define START_BUTTON_GPIO_S2_PIN                                (DL_GPIO_PIN_21)
#define START_BUTTON_GPIO_S2_IOMUX                               (IOMUX_PINCM49)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_MOTOR_PWM_init(void);
void SYSCFG_DL_UART_0_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
