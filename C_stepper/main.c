/*
 * main.c  --  步进电机控球板 (MSPM0G3507)
 *
 * SysConfig: UART0(PA10=RX,PA11=TX), TIMG12(双PWM PA5/PA6), TIMG6(10ms)
 */
#include "ti_msp_dl_config.h"
#include "stepper.h"

#define LED_PORT  GPIOB
#define LED_PIN   DL_GPIO_PIN_22

int main(void)
{
    SYSCFG_DL_init();
    DL_GPIO_setPins(LED_PORT, LED_PIN);
    Stepper_Init();
    DL_TimerG_startCounter(CONTROL_TIMER);
    while (1) { __WFI(); }
}

void UART_0_INST_IRQHandler(void)
{
    if (DL_UART_Main_getPendingInterrupt(UART_0_INST) == DL_UART_MAIN_IIDX_RX) {
        while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
            Stepper_UART_RX(DL_UART_Main_receiveData(UART_0_INST));
        }
    }
}

void TIMG6_IRQHandler(void)
{
    if (DL_TimerG_getPendingInterrupt(CONTROL_TIMER) & DL_TIMER_IIDX_ZERO) {
        Stepper_Tick();
    }
}
