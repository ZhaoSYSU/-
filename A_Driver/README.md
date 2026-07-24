# A_Driver — MSPM0G3507 底层驱动

## 文件说明

| 文件 | 说明 |
|------|------|
| `empty.c` | 主程序：UART 中断接收 + K230 帧解析 + 巡线差速控制 + OLED 显示 |
| `empty.syscfg` | SysConfig 图形化配置，用 CCS 打开可编辑 |
| `ti_msp_dl_config.c/.h` | SysConfig 自动生成的初始化代码 |
| `motor.c/.h` | 双路直流电机驱动（TB6612FNG）+ 增量式 PI 速度环 |
| `encoder.c/.h` | 双路编码器软件解码（MG310）+ 速度反馈 + UART 帧超时检测 |
| `oled.c/.h` | SSD1306 OLED 128×64 SPI 显示 |

## 硬件平台

- 主控: TI MSPM0G3507 LaunchPad
- 电机: TB6612FNG 双路驱动
- 编码器: MG310 霍尔编码器 (20PPR, 减速比 13:1, 520CPR)
- 轮径: 48mm, 周长 150.72mm
- 显示: SSD1306 128×64 OLED (SPI)
- 通信: UART0 (PA8=RX, PA9=TX, 115200 8N1)

## 定时器分配

| 定时器 | 功能 | 周期 |
|--------|------|------|
| TIMG0 | 双路 PWM 输出 | 10kHz |
| TIMG12 | 编码器轮询 + UART 帧超时检测 | 1ms |
| TIMG6 | 速度计算 + PI 速度环更新 | 100ms |

## 速度控制

增量式 PI 算法，每 100ms 更新一次：

```
Δu = Kp × (e[k] - e[k-1]) + Ki × e[k]
CC = CC_old - Δu  （CC↓ → 占空比↑ → 加速）
```

| 参数 | 值 | 说明 |
|------|-----|------|
| Kp | 3.0 | 比例系数 (`PID_KP=30 / 10`) |
| Ki | 5.0 | 积分系数 (`PID_KI=50 / 10`) |
| du_max | ±80 | 单次增量限幅 |
| CC 范围 | 0~3199 | PWM 比较值 |

### 速度公式

```
速度(mm/s) = 脉冲数 × 942 ÷ 325
           = 脉冲数 × 150.72mm ÷ 520CPR ÷ 0.1s
```

## API

```c
// 电机
Motor_Init()                              // 初始化
Motor_SetDir(MOTOR_A, MOTOR_FWD)          // 方向: FWD/REV/STOP
Motor_SetSpeed(MOTOR_A, cc)               // 速度 (CC值, 0=最快)
Motor_Enable()                            // 使能驱动
Motor_SetTarget(MOTOR_A, speed_mms)       // 设目标速度 mm/s
Motor_PID_Update(speedA, speedB)          // PI 更新 (TIMG6 自动调用)

// 编码器
Encoder_GetSpeed_A() / _B()               // 当前速度 mm/s
Encoder_GetPulse_A() / _B()               // 累计脉冲

// OLED
OLED_Clear() / OLED_ShowString() / OLED_Refresh()
```

## K230 视觉数据接入

UART0 中断接收 K230 发来的视觉帧，主循环消费：

```
K230 帧格式: [0xAA][CMD][DH][DL][CS][0x55]
  CMD 0x01 → 巡线偏差 → 差速控制 (base ± dev × gain)
  CMD 0x02 → 数字识别 → 存 gVis.digit
  CMD 0x03 → 视觉状态 → status=2 时停车
  CMD 0x04 → 轨道类型 → 存 gVis.track

接收: UART0 RX 中断 → 逐字节组帧 → 验 CS → 解析 → gVis
控制: 主循环读 gVis.deviation → Motor_SetTarget(A, base+diff) (B, base-diff)
超时: TIMG12 每 1ms 检查，10ms 收不齐帧则丢弃重找帧头
```

## OLED 显示内容

```
V A: 00040    ← 电机A 速度 mm/s
V B: 00040    ← 电机B 速度 mm/s
D:+03 N:3 TS  ← 偏差+03 / 数字3 / 轨道Straight
               末尾 ! 表示丢线
```

## 引脚接线

### MSPM0 → 电机驱动/编码器/OLED

| MSPM0 | 外设 | 说明 |
|-------|------|------|
| PA5 | TB6612 AIN1 | 电机A方向 (原PA8, 让给UART) |
| PA15 | TB6612 AIN2 | 电机A方向 |
| PA12 | TB6612 PWMA | 电机A PWM |
| PB13 | TB6612 BIN1 | 电机B方向 |
| PB12 | TB6612 BIN2 | 电机B方向 |
| PA13 | TB6612 PWMB | 电机B PWM |
| PB24 | TB6612 STBY | 使能 |
| PA17/PA18 | MG310 E1A/E1B | 编码器A |
| PB4/PB15 | MG310 E2A/E2B | 编码器B |
| PB9/PB8/PB3/PB2/PA27 | SSD1306 SPI | OLED |

### MSPM0 ← K230 UART

| MSPM0 | K230 | 说明 |
|-------|------|------|
| PA8 (UART0 RX) | GPIO40 (UART1 TX) | 接收视觉数据 |
| PA9 (UART0 TX) | GPIO41 (UART1 RX) | 发送反馈（可选） |
| GND | GND | 共地 |
