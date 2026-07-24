# A_Driver — MSPM0G3507 底层驱动

## 文件说明

| 文件 | 说明 |
|------|------|
| `empty.c` | 主程序入口 + 引脚接线总表 |
| `empty.syscfg` | SysConfig 图形化配置（时钟/外设/引脚），用 CCS 打开可图形编辑 |
| `ti_msp_dl_config.c/.h` | SysConfig 自动生成的初始化代码 |
| `motor.c/.h` | 双路直流电机驱动（TB6612FNG）+ 增量式 PI 速度环 |
| `encoder.c/.h` | 双路编码器软件解码（MG310）+ 速度反馈 |
| `oled.c/.h` | SSD1306 OLED 128×64 SPI 显示 |

## 硬件平台

- 主控: TI MSPM0G3507 LaunchPad
- 电机: TB6612FNG 双路驱动
- 编码器: MG310 霍尔编码器 (20PPR, 减速比 13:1, 520CPR)
- 轮径: 48mm, 周长 150.72mm
- 显示: SSD1306 128×64 OLED (SPI)

## 定时器分配

| 定时器 | 功能 | 周期 |
|--------|------|------|
| TIMG0 | 双路 PWM 输出 | 10kHz |
| TIMG12 | 编码器轮询 | 1ms |
| TIMG6 | 速度计算 + PID 更新 | 100ms |

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
Motor_PID_Update(speedA, speedB)          // PI 更新 (TIMG6自动调用)

// 编码器
Encoder_GetSpeed_A() / _B()               // 当前速度 mm/s
Encoder_GetPulse_A() / _B()               // 累计脉冲

// OLED
OLED_Clear() / OLED_ShowString() / OLED_Refresh()
```

## K230 视觉数据接入

在 `empty.c` 的 main 循环里新增 UART 中断接收，解析 K230 发来的视觉帧：

```
[0xAA][CMD][DH][DL][CS][0x55]

CMD 0x01 → 巡线偏差 → Motor_SetTarget(A, base+偏差) + (B, base-偏差)
CMD 0x02 → 数字识别 → 存到全局变量，状态机判断是否目标病房
CMD 0x03 → 视觉状态 → 0=正常, 2=丢线→减速/停车
CMD 0x04 → 轨道类型 → 触发转弯决策
```

UART 接收在 SysConfig 中配置 PA8=RX, PA9=TX（UART1），然后写中断回调。
