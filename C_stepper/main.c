/*
 * main.c  --  步进电机控球板 (MSPM0G3507)
 *
 * 功能: 接收主控板 UART 数据 → 双PID+前馈 → 步进电机
 *
 * SysConfig 配置:
 *   - UART0: PA10=RX, PA11=TX (收主控板数据)
 *   - TIMG12: 双路 PWM, CC0=左STEP(PA5), CC1=右STEP(PA6)
 *   - TIMG6: 10ms 控制定时器
 *   - GPIO: PA5=左DIR, PA6=右DIR
 */
#include "ti_msp_dl_config.h"
#include "stepper.h"
#include <string.h>

/* 面板 LED (PB22, 同 A_Driver) */
#define LED_PORT  GPIOB
#define LED_PIN   DL_GPIO_PIN_22

int main(void)
{
    SYSCFG_DL_init();

    /* 点灯: 程序已启动 */
    DL_GPIO_setPins(LED_PORT, LED_PIN);

    Stepper_Init();
    DL_TimerG_startCounter(CONTROL_TIMER);

    while (1) {
        __WFI();  /* 休眠, 等中断唤醒 */
    }
}

/* ---- UART 中断 ---- */
void UART_0_INST_IRQHandler(void)
{
    if (DL_UART_Main_getPendingInterrupt(UART_INST) == DL_UART_MAIN_IIDX_RX) {
        while (!DL_UART_Main_isRXFIFOEmpty(UART_INST)) {
            Stepper_UART_RX(DL_UART_Main_receiveData(UART_INST));
        }
    }
}

/* ---- 控制定时器中断 (10ms) ---- */
void TIMG6_IRQHandler(void)
{
    if (DL_TimerG_getPendingInterrupt(CONTROL_TIMER) & DL_TIMER_IIDX_ZERO) {
        Stepper_Tick();
    }
}
