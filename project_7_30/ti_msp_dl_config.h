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
#define MOTOR_PWM_INST                                                    TIMG12
#define UART_0_INST                                                        UART0
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define ENCODER_GPIO_INT_IRQN                                  GPIOB_INT_IRQn
#define ENCODER_GPIO_INT_IIDX                  (DL_INTERRUPT_GROUP1_IIDX_GPIOB)




/* Port definition for Pin Group TRACK_GPIO */
#define TRACK_GPIO_PORT                                                  (GPIOA)

/* Defines for TRACK_1: GPIOA.17 with pinCMx 39 on package pin 10 */
#define TRACK_1_PIN                                              (DL_GPIO_PIN_17)
#define TRACK_1_IOMUX                                             (IOMUX_PINCM39)
/* Defines for TRACK_2: GPIOA.18 with pinCMx 40 on package pin 11 */
#define TRACK_2_PIN                                              (DL_GPIO_PIN_18)
#define TRACK_2_IOMUX                                             (IOMUX_PINCM40)
/* Defines for TRACK_3: GPIOA.2 with pinCMx 3 on package pin 4 */
#define TRACK_3_PIN                                               (DL_GPIO_PIN_2)
#define TRACK_3_IOMUX                                              (IOMUX_PINCM3)
/* Defines for TRACK_4: GPIOA.3 with pinCMx 4 on package pin 5 */
#define TRACK_4_PIN                                               (DL_GPIO_PIN_3)
#define TRACK_4_IOMUX                                              (IOMUX_PINCM4)
/* Defines for TRACK_5: GPIOA.4 with pinCMx 5 on package pin 6 */
#define TRACK_5_PIN                                               (DL_GPIO_PIN_4)
#define TRACK_5_IOMUX                                              (IOMUX_PINCM5)
/* Defines for TRACK_6: GPIOA.5 with pinCMx 6 on package pin 7 */
#define TRACK_6_PIN                                               (DL_GPIO_PIN_5)
#define TRACK_6_IOMUX                                              (IOMUX_PINCM6)
/* Defines for TRACK_7: GPIOA.6 with pinCMx 7 on package pin 8 */
#define TRACK_7_PIN                                               (DL_GPIO_PIN_6)
#define TRACK_7_IOMUX                                              (IOMUX_PINCM7)
/* Defines for TRACK_8: GPIOA.7 with pinCMx 8 on package pin 9 */
#define TRACK_8_PIN                                               (DL_GPIO_PIN_7)
#define TRACK_8_IOMUX                                              (IOMUX_PINCM8)
/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);


#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
