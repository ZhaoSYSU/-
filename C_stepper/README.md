# C_stepper — 步进电机控球板 (MSPM0G3507)

## 文件

| 文件 | 用途 |
|------|------|
| `main.c` | 主程序: SYSCFG_DL_init → UART中断 + TIMG6控制循环 |
| `stepper.h` | 双 PID + 前馈参数、API |
| `stepper.c` | UART 帧解析 + PID 切换 + 前馈 + 步进 PWM |

## 硬件

| 信号 | 引脚 | 说明 |
|------|------|------|
| UART RX | PA10 | ← 主控板 UART1 TX (PA8) |
| UART TX | PA11 | → 主控板 UART1 RX (PA9, 可选) |
| 左 DIR | PA5 | 步进方向 |
| 左 STEP | PA0 (TIMG12 CC0) | 步进脉冲 |
| 右 DIR | PA6 | 步进方向 |
| 右 STEP | PA1 (TIMG12 CC1) | 步进脉冲 |

## SysConfig 配置

- UART0: PA10=RX, PA11=TX, 115200 8N1, RX中断
- TIMG12: 双路 PWM, CC0=PA0, CC1=PA1
- TIMG6: 10ms 周期定时器
- GPIO: PA5/PA6 输出, PB22=LED

## UART 协议

```
[0xAA][CMD][DH][DL][CS][0x55]
CMD 0x01 = 球X坐标 mm (int16)
CMD 0x05 = track(1B) + speed_cms(1B)
```

## PID 参数 (占位)

| 参数 | 直道 | 弯道 |
|------|------|------|
| KP | 1.0 | 1.0 |
| KI | 0 | 0 |
| KD | 0 | 0 |
| OutMax | 6000 | 7000 Hz |
