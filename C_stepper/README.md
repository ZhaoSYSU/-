# C_stepper — 步进电机控球板 (STM32F103RCT6)

## 文件

| 文件 | 用途 |
|------|------|
| `control.h` | 双 PID + 前馈参数、API |
| `control.c` | UART 帧解析 + PID 切换 + 前馈 + 步进驱动 |

> 依赖工程: `E:\文档资料\步进电机资料\1.STM32F103RCT6控制D36A驱动双路步进电机`
> 把这个目录下所有 `.c/.h` 拷贝进原始工程的 `USER/` 文件夹

## 硬件

| 部件 | 型号 | 引脚 |
|------|------|------|
| 主控 | STM32F103RCT6 | – |
| 步进驱动 | ATD5984 ×2 | – |
| 左 STEP | PC8 | TIM8 CH3 |
| 左 DIR | PC13 | GPIO |
| 左 SLEEP | PD2 | GPIO |
| 右 STEP | PC9 | TIM8 CH4 |
| 右 DIR | PC14 | GPIO |
| 右 SLEEP | PC12 | GPIO |
| UART RX | PA10 | ← 主控板 TX |

## 接入原始工程

1. `control.c/h` 拷进 `USER/`
2. `main.c`: `#include "control.h"`, `Control_Init();`
3. `stm32f10x_it.c`: 加 `USART1_IRQHandler`，调 `UART_RX_Handler()`
4. `TIM.c`: `TIM2_IRQHandler` 里加 `Control_Tick();`

## PID 参数 (占位, 现场标定)

| 参数 | 直道 | 弯道 |
|------|------|------|
| KP | 1.2 | 1.0 |
| KI | 0.15 | 0.20 |
| KD | 0.8 | 1.0 |
| OutMax | 6000 | 7000 Hz |
