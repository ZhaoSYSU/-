# 智能送药小车 — 2026电赛F题

## 仓库结构

```
/
├── B_Vision/              # K230D 视觉模块
│   ├── vision_line.py     # 主程序（巡线 + 数字识别 + UART通信）
│   ├── vision_full.py     # 备份方案（双KPU识别器，无巡线）
│   ├── yolo_marble.py     # 钢珠检测
│   ├── yolo_number.py     # YOLO数字检测（需训练kmodel）
│   └── clean_features.py  # 重训练前清理特征文件
│
└── A_Driver/              # MSPM0G3507 底层驱动
    ├── empty.c            # 主程序（UART帧接收 + 巡线控制 + OLED）
    ├── empty.syscfg       # SysConfig 配置
    ├── motor.c / motor.h  # 双路直流电机驱动 + PI速度环
    ├── encoder.c / encoder.h  # 编码器测速 + UART帧超时
    ├── oled.c / oled.h    # SSD1306 OLED
    └── ti_msp_dl_config.c/.h  # SysConfig 自动生成
```

## 硬件架构

```
K230D UART1(GPIO40/41) ──▶ MSPM0 UART0(PA8/PA9) ──▶ PI速度环 ──▶ 双电机
    (视觉)                  (主控)                (motor.c)    TB6612FNG
                                │
                                ├── TIMG12 1ms ──▶ 编码器轮询 (MG310)
                                ├── TIMG6 100ms ──▶ 速度计算+PI
                                └── SSD1306 OLED
```

| 模块 | 型号 |
|------|------|
| 视觉 | K230D (DNK230D, GC2093摄像头) |
| 主控 | MSPM0G3507 |
| 电机驱动 | TB6612FNG |
| 编码器 | MG310 (20PPR, 减速比13:1, 520CPR) |
| 轮径 | 48mm, 周长 150.72mm |
| 显示 | SSD1306 128×64 SPI |

## 通信协议

```
K230 UART1 (GPIO40=TX, GPIO41=RX) → MSPM0 UART0 (PA8=RX, PA9=TX)
115200 8N1

帧格式: [0xAA][CMD][DH][DL][CS][0x55]
  CMD 0x01 = 巡线偏差 (int16, -160 ~ +160)
  CMD 0x02 = 识别数字 (1 ~ 8)
  CMD 0x03 = 视觉状态 (0=正常, 2=丢线)
  CMD 0x04 = 轨道类型 (straight=0, left=-80, right=80)

CHECKSUM = (CMD + DH + DL) & 0xFF
```

## 引脚接线

### K230D → MSPM0

| K230 | MSPM0 | 说明 |
|------|-------|------|
| GPIO40 (UART1 TX) | PA8 (UART0 RX) | 视觉数据 |
| GPIO41 (UART1 RX) | PA9 (UART0 TX) | 反馈（可选） |
| GND | GND | 共地 |

### MSPM0 → 外设

| MSPM0 | 外设 | 说明 |
|-------|------|------|
| PA5 | TB6612 AIN1 | 电机A方向 (原PA8,让给UART) |
| PA15 | TB6612 AIN2 | 电机A方向 |
| PA12 | TB6612 PWMA | 电机A PWM |
| PB13 | TB6612 BIN1 | 电机B方向 |
| PB12 | TB6612 BIN2 | 电机B方向 |
| PA13 | TB6612 PWMB | 电机B PWM |
| PB24 | TB6612 STBY | 使能 |
| PA17/PA18 | MG310 E1A/E1B | 编码器A |
| PB4/PB15 | MG310 E2A/E2B | 编码器B |

## 开发流程

### K230 (CanMV IDE)
1. IDE 打开 `B_Vision/vision_line.py`
2. 首次/重训: 先跑 `clean_features.py`
3. 运行 `vision_line.py`, LCD 应看到摄像头画面 + 巡线状态

### MSPM0 (CCS)
1. CCS 打开工程 (workspace_ccstheia)
2. 编译前把 `A_Driver/` 下的改动拷到 CCS 工程目录
3. SysConfig 改了也要拷回 E 盘仓库
4. 编译 → 烧录

### 联调
1. 接好 UART 线 (K230 GPIO40/41 ↔ MSPM0 PA8/PA9)
2. K230 先跑 `vision_line.py`
3. MSPM0 上电，OLED 第三行应显示 `D: N: T:`
4. K230 前放数字卡，OLED 上 `N:` 应变
5. 上赛道调 `kBaseSpeed` / `kTurnGain`
