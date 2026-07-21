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
 *  DO NOT EDIT - This file is generated for the LP_MSPM0G3507
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_LP_MSPM0G3507
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



/* Defines for PWM_MOTOR */
#define PWM_MOTOR_INST                                                     TIMG0
#define PWM_MOTOR_INST_IRQHandler                                   TIMG0_IRQHandler
#define PWM_MOTOR_INST_INT_IRQN                                     (TIMG0_INT_IRQn)
#define PWM_MOTOR_INST_CLK_FREQ                                           4000000
/* GPIO defines for channel 0 (PA12) */
#define GPIO_PWM_MOTOR_C0_PORT                                                 GPIOA
#define GPIO_PWM_MOTOR_C0_PIN                                         DL_GPIO_PIN_12
#define GPIO_PWM_MOTOR_C0_IOMUX                                      (IOMUX_PINCM34)
#define GPIO_PWM_MOTOR_C0_IOMUX_FUNC                     IOMUX_PINCM34_PF_TIMG0_CCP0
#define GPIO_PWM_MOTOR_C0_IDX                                    DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 (PA13) */
#define GPIO_PWM_MOTOR_C1_PORT                                                 GPIOA
#define GPIO_PWM_MOTOR_C1_PIN                                         DL_GPIO_PIN_13
#define GPIO_PWM_MOTOR_C1_IOMUX                                      (IOMUX_PINCM35)
#define GPIO_PWM_MOTOR_C1_IOMUX_FUNC                     IOMUX_PINCM35_PF_TIMG0_CCP1
#define GPIO_PWM_MOTOR_C1_IDX                                    DL_TIMER_CC_1_INDEX


/* Port definition for Pin Group GPIO_MOTOR */
#define GPIO_MOTOR_PORT                                                   (GPIOB)

/* Defines for AIN1: GPIOB.16 */
#define GPIO_MOTOR_AIN1_PIN                                      (DL_GPIO_PIN_16)
#define GPIO_MOTOR_AIN1_IOMUX                                     (IOMUX_PINCM33)
/* Defines for AIN2: GPIOB.13 */
#define GPIO_MOTOR_AIN2_PIN                                      (DL_GPIO_PIN_13)
#define GPIO_MOTOR_AIN2_IOMUX                                     (IOMUX_PINCM30)
/* Defines for BIN1: GPIOB.3 */
#define GPIO_MOTOR_BIN1_PIN                                        (DL_GPIO_PIN_3)
#define GPIO_MOTOR_BIN1_IOMUX                                     (IOMUX_PINCM16)
/* Defines for BIN2: GPIOB.2 */
#define GPIO_MOTOR_BIN2_PIN                                        (DL_GPIO_PIN_2)
#define GPIO_MOTOR_BIN2_IOMUX                                     (IOMUX_PINCM15)


/* Port definition for Pin Group GPIO_SWITCHES */
#define GPIO_SWITCHES_PORT                                               (GPIOB)

/* Defines for USER_SWITCH_1 (S2): GPIOB.21 */
#define GPIO_SWITCHES_USER_SWITCH_1_PIN                         (DL_GPIO_PIN_21)
#define GPIO_SWITCHES_USER_SWITCH_1_IOMUX                        (IOMUX_PINCM49)
/* Defines for ENC1A: GPIOA.17 */
#define GPIO_SWITCHES_ENC1A_PIN                                  (DL_GPIO_PIN_17)
#define GPIO_SWITCHES_ENC1A_IOMUX                                (IOMUX_PINCM39)
/* Defines for ENC1B: GPIOA.18 */
#define GPIO_SWITCHES_ENC1B_PIN                                  (DL_GPIO_PIN_18)
#define GPIO_SWITCHES_ENC1B_IOMUX                                (IOMUX_PINCM40)
/* Defines for ENC2A: GPIOB.6 */
#define GPIO_SWITCHES_ENC2A_PIN                                   (DL_GPIO_PIN_6)
#define GPIO_SWITCHES_ENC2A_IOMUX                                (IOMUX_PINCM19)
/* Defines for ENC2B: GPIOB.7 */
#define GPIO_SWITCHES_ENC2B_PIN                                   (DL_GPIO_PIN_7)
#define GPIO_SWITCHES_ENC2B_IOMUX                                (IOMUX_PINCM20)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_PWM_MOTOR_init(void);


#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
