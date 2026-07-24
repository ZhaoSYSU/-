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



/* Defines for PWMA */
#define PWMA_INST                                                          TIMG0
#define PWMA_INST_IRQHandler                                    TIMG0_IRQHandler
#define PWMA_INST_INT_IRQN                                      (TIMG0_INT_IRQn)
#define PWMA_INST_CLK_FREQ                                              32000000
/* GPIO defines for channel 0 */
#define GPIO_PWMA_C0_PORT                                                  GPIOA
#define GPIO_PWMA_C0_PIN                                          DL_GPIO_PIN_12
#define GPIO_PWMA_C0_IOMUX                                       (IOMUX_PINCM34)
#define GPIO_PWMA_C0_IOMUX_FUNC                      IOMUX_PINCM34_PF_TIMG0_CCP0
#define GPIO_PWMA_C0_IDX                                     DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWMA_C1_PORT                                                  GPIOA
#define GPIO_PWMA_C1_PIN                                          DL_GPIO_PIN_13
#define GPIO_PWMA_C1_IOMUX                                       (IOMUX_PINCM35)
#define GPIO_PWMA_C1_IOMUX_FUNC                      IOMUX_PINCM35_PF_TIMG0_CCP1
#define GPIO_PWMA_C1_IDX                                     DL_TIMER_CC_1_INDEX



/* Defines for UART_0 */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           32000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                         DL_GPIO_PIN_1
#define GPIO_UART_0_TX_PIN                                         DL_GPIO_PIN_0
#define GPIO_UART_0_IOMUX_RX                                      (IOMUX_PINCM2)
#define GPIO_UART_0_IOMUX_TX                                      (IOMUX_PINCM1)
#define GPIO_UART_0_IOMUX_RX_FUNC                       IOMUX_PINCM2_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                       IOMUX_PINCM1_PF_UART0_TX
#define UART_0_BAUD_RATE                                                (115200)
#define UART_0_IBRD_32_MHZ_115200_BAUD                                      (17)
#define UART_0_FBRD_32_MHZ_115200_BAUD                                      (23)




/* Defines for OLED_SPI */
#define OLED_SPI_INST                                                      SPI1
#define OLED_SPI_INST_IRQHandler                                SPI1_IRQHandler
#define OLED_SPI_INST_INT_IRQN                                    SPI1_INT_IRQn
#define GPIO_OLED_SPI_PICO_PORT                                           GPIOB
#define GPIO_OLED_SPI_PICO_PIN                                    DL_GPIO_PIN_8
#define GPIO_OLED_SPI_IOMUX_PICO                                (IOMUX_PINCM25)
#define GPIO_OLED_SPI_IOMUX_PICO_FUNC                IOMUX_PINCM25_PF_SPI1_PICO
#define GPIO_OLED_SPI_POCI_PORT                                           GPIOA
#define GPIO_OLED_SPI_POCI_PIN                                   DL_GPIO_PIN_16
#define GPIO_OLED_SPI_IOMUX_POCI                                (IOMUX_PINCM38)
#define GPIO_OLED_SPI_IOMUX_POCI_FUNC                IOMUX_PINCM38_PF_SPI1_POCI
/* GPIO configuration for OLED_SPI */
#define GPIO_OLED_SPI_SCLK_PORT                                           GPIOB
#define GPIO_OLED_SPI_SCLK_PIN                                    DL_GPIO_PIN_9
#define GPIO_OLED_SPI_IOMUX_SCLK                                (IOMUX_PINCM26)
#define GPIO_OLED_SPI_IOMUX_SCLK_FUNC                IOMUX_PINCM26_PF_SPI1_SCLK
#define GPIO_OLED_SPI_CS0_PORT                                            GPIOA
#define GPIO_OLED_SPI_CS0_PIN                                     DL_GPIO_PIN_2
#define GPIO_OLED_SPI_IOMUX_CS0                                  (IOMUX_PINCM7)
#define GPIO_OLED_SPI_IOMUX_CS0_FUNC                   IOMUX_PINCM7_PF_SPI1_CS0



/* Defines for AIN1: GPIOA.8 with pinCMx 19 on package pin 54 */
#define DC_MOTOR_AIN1_PORT                                               (GPIOA)
#define DC_MOTOR_AIN1_PIN                                        (DL_GPIO_PIN_8)
#define DC_MOTOR_AIN1_IOMUX                                      (IOMUX_PINCM19)
/* Defines for AIN2: GPIOA.15 with pinCMx 37 on package pin 8 */
#define DC_MOTOR_AIN2_PORT                                               (GPIOA)
#define DC_MOTOR_AIN2_PIN                                       (DL_GPIO_PIN_15)
#define DC_MOTOR_AIN2_IOMUX                                      (IOMUX_PINCM37)
/* Defines for STBY: GPIOB.24 with pinCMx 52 on package pin 23 */
#define DC_MOTOR_STBY_PORT                                               (GPIOB)
#define DC_MOTOR_STBY_PIN                                       (DL_GPIO_PIN_24)
#define DC_MOTOR_STBY_IOMUX                                      (IOMUX_PINCM52)
/* Defines for BIN1: GPIOB.13 with pinCMx 30 on package pin 1 */
#define DC_MOTOR_BIN1_PORT                                               (GPIOB)
#define DC_MOTOR_BIN1_PIN                                       (DL_GPIO_PIN_13)
#define DC_MOTOR_BIN1_IOMUX                                      (IOMUX_PINCM30)
/* Defines for BIN2: GPIOB.12 with pinCMx 29 on package pin 64 */
#define DC_MOTOR_BIN2_PORT                                               (GPIOB)
#define DC_MOTOR_BIN2_PIN                                       (DL_GPIO_PIN_12)
#define DC_MOTOR_BIN2_IOMUX                                      (IOMUX_PINCM29)
/* Defines for OLED_RES: GPIOB.3 with pinCMx 16 on package pin 51 */
#define DC_MOTOR_OLED_RES_PORT                                           (GPIOB)
#define DC_MOTOR_OLED_RES_PIN                                    (DL_GPIO_PIN_3)
#define DC_MOTOR_OLED_RES_IOMUX                                  (IOMUX_PINCM16)
/* Defines for OLED_DC: GPIOB.2 with pinCMx 15 on package pin 50 */
#define DC_MOTOR_OLED_DC_PORT                                            (GPIOB)
#define DC_MOTOR_OLED_DC_PIN                                     (DL_GPIO_PIN_2)
#define DC_MOTOR_OLED_DC_IOMUX                                   (IOMUX_PINCM15)
/* Defines for OLED_CS: GPIOA.27 with pinCMx 60 on package pin 31 */
#define DC_MOTOR_OLED_CS_PORT                                            (GPIOA)
#define DC_MOTOR_OLED_CS_PIN                                    (DL_GPIO_PIN_27)
#define DC_MOTOR_OLED_CS_IOMUX                                   (IOMUX_PINCM60)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_PWMA_init(void);
void SYSCFG_DL_UART_0_init(void);
void SYSCFG_DL_OLED_SPI_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
