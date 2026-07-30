# C_stepper — 步进电机控球板

## 文件

| 文件 | 用途 |
|------|------|
| `main.c` | 主程序: UART中断 + TIMG控制循环 |
| `stepper.h` | 接口: 双PID参数 + API |
| `stepper.c` | 实现: UART帧解析 + PID切换 + 前馈 + 步进驱动 |

## 架构

```
主控板(A_Driver) ──UART1──▶ 本板(UART0)
  CMD 0x01: 球X坐标 mm
  CMD 0x05: track_type + speed_cms
```

## 硬件接线

| 信号 | MSPM0 引脚 | 说明 |
|------|-----------|------|
| UART RX | PA10 | ← 主控板 PA8 (UART1 TX) |
| UART TX | PA11 | → 主控板 PA9 (UART1 RX, 可选) |
| 左 DIR | PA5 | 步进驱动方向 |
| 左 STEP | PA0 (TIMG12 CC0) | 步进脉冲 |
| 右 DIR | PA6 | 步进驱动方向 |
| 右 STEP | PA1 (TIMG12 CC1) | 步进脉冲 |
| GND | GND | 共地 |

## UART 协议

```
[0xAA][CMD][DH][DL][CS][0x55]
CMD 0x01 = 球X坐标 mm (int16, 大端)
CMD 0x05 = track(1B) + speed_cms(1B)
  track: 0=直道, 1=弯道
  speed: 平均车速 cm/s
```

## PID 参数 (待现场标定)

所有参数均为占位值，标定后填入。

| 参数 | 直道 | 弯道 | 说明 |
|------|------|------|------|
| KP | 1000 | 1000 | ×PID_SCALE, 实际=1.0 |
| KI | 0 | 0 | 先不启用 |
| KD | 0 | 0 | 先不启用 |
| OutMax | 6000 | 7000 | 步进频率 Hz |

## API

```c
void Stepper_Init(void);                       /* 初始化 */
void Stepper_Tick(void);                       /* 每10ms, TIMG6中断 */
void Stepper_UART_RX(uint8_t byte);            /* UART中断喂字节 */
void Stepper_SetDir(int motor, int dir);       /* 0=左 1=右, 0=正 1=反 */
void Stepper_SetFreq(int motor, int hz);       /* 200~8000Hz */
void Stepper_Stop(void);                       /* 停车 */
```
