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
    ├── motor.c / motor.h  # 双路直流电机驱动（PWM + PI速度环）
    ├── encoder.c / encoder.h  # 编码器测速（MG310, 20PPR, 减速比13:1）
    ├── oled.c / oled.h    # SSD1306 OLED显示
    └── empty.c            # main, 引脚接线总表
```

## 硬件架构

```
K230D (视觉)  ──UART1──▶  MSPM0G3507 (主控)  ──PWM──▶  TB6612FNG ──▶  双电机
                  ▲                      │
                  │                      ├── QEI ──▶  编码器A/B (MG310)
                  │                      │
                  └──── UART1 ◀──────────┘        ──▶  SSD1306 OLED
```

| 模块 | 型号 |
|------|------|
| 视觉 | K230D (DNK230D, GC2093摄像头) |
| 主控 | MSPM0G3507 |
| 电机驱动 | TB6612FNG |
| 编码器 | MG310 (20PPR, 减速比13:1) |
| 轮径 | 48mm, 周长150.72mm |
| 显示 | SSD1306 128×64 SPI |

## 通信协议

K230 → MSPM0, UART1 (GPIO40=TXD, GPIO41=RXD), 115200 8N1

```
帧格式: [0xAA][CMD][DH][DL][CHECKSUM][0x55]

CMD 0x01 = 巡线偏差 (int16, -160 ~ +160 像素)
CMD 0x02 = 识别数字 (1 ~ 8)
CMD 0x03 = 视觉状态 (0=正常巡线, 2=丢线)
CMD 0x04 = 轨道类型 (0=直行, -80=左转, 80=右转)

CHECKSUM = (CMD + DH + DL) & 0xFF
```

## 引脚接线

### K230D → MSPM0

| K230 | MSPM0 | 说明 |
|------|-------|------|
| GPIO40 (UART1 TX) | PA9 (UART1 RX) | 视觉数据发送 |
| GPIO41 (UART1 RX) | PA8 (UART1 TX) | 主控反馈（可选） |
| GND | GND | 共地 |

### MSPM0 → 电机驱动

| MSPM0 | TB6612FNG | 说明 |
|-------|-----------|------|
| PA8 | AIN1 | 电机A方向 |
| PA15 | AIN2 | 电机A方向 |
| PA12 | PWMA | 电机A速度 |
| PB13 | BIN1 | 电机B方向 |
| PB12 | BIN2 | 电机B方向 |
| PA13 | PWMB | 电机B速度 |
| PB24 | STBY | 使能 |

### MSPM0 → 编码器

| MSPM0 | MG310 | 说明 |
|-------|-------|------|
| PA17 | E1A | 编码器A相 |
| PA18 | E1B | 编码器B相 |
| PB4 | E2A | 编码器B相 |
| PB15 | E2B | 编码器B相 |

## 开发流程

1. **K230单独调试**: IDE运行 `vision_line.py`, 确认巡线/数字识别/LCD显示正常
2. **MSPM0单独调试**: CCS编译 `A_Driver`, 确认电机/编码器/OLED正常
3. **联调**: 接UART, K230发巡线偏差, MSPM0做PID控制
4. **场地实测**: 上赛道调巡线ROI/阈值/PID参数
