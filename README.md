# 车载平衡滚球运动控制系统 — 2026 年 TI 杯 H 题

> 仓库名 `medicine_car` 来自赛前练习"智能送药小车"。实际参赛为 **H 题：车载平衡滚球运动控制系统**。

## 1. 系统链路

```
K230D 视觉识别 ──UART──▶ 主控板 MSPM0G3507 (A_Driver)
   球 X 坐标                     │
                         巡线+编码器+电机PID
                                 │
                          UART1 ──▶ 步进板 MSPM0G3507 (C_stepper)
                           球坐标+车速+轨道         │
                                           双PID+前馈 → 步进电机 → 摆杆
```

## 2. 目录结构

```
/
├── A_Driver/              # 主控板 — 巡线+编码器+直流电机
│   ├── empty.c            # 主程序 (PID+巡线+UART收发+步进板通信)
│   ├── empty.syscfg       # SysConfig 配置
│   ├── ti_msp_dl_config.c/.h
│   └── pin_connections.md # 引脚接线表
│
├── B_Vision/              # K230D 视觉模块
│   ├── main.py            # 触摸标定零轴 + YOLO钢球识别 + UART输出
│   └── README.md
│
├── C_stepper/             # 步进电机控球板
│   ├── main.c             # UART中断 + TIMG6控制循环
│   ├── stepper.h          # 双PID参数(占位) + API
│   ├── stepper.c          # 帧解析 + PID切换 + 前馈 + 步进驱动
│   └── README.md
│
├── H题_车载平衡滚球运动控制系统.pdf
└── README.md
```

## 3. 通信协议

### 3.1 K230D → 主控板

```
[0xAA][0x01][DH][DL][CS][0x55]
CMD 0x01 = 球 X 坐标 mm (int16, 大端, 右正左负)
CS = (0x01 + DH + DL) & 0xFF
```

### 3.2 主控板 → 步进板

```
[0xAA][CMD][DH][DL][CS][0x55]

CMD 0x01 = 球 X 坐标 mm (主控板转发 K230 数据)
CMD 0x05 = 车速 + 轨道类型
  DH = track_type: 0=直道, 1=弯道
  DL = speed_cms: 平均车速 cm/s
```

## 4. 模块说明

### 4.1 A_Driver (主控板)

| 功能 | 状态 |
|------|------|
| 八路红外巡线 | ✅ |
| 左右直流电机 PI 速度环 + 编码器 | ✅ |
| 接收 K230 球 X 坐标 (CMD 0x01) | ✅ |
| 转发球坐标给步进板 (UART1) | ✅ |
| 计算车速+轨道类型, 发给步进板 (CMD 0x05) | ✅ |
| SSD1306 OLED 显示 | ✅ |

**PID 参数在 `empty.c` 第 39-61 行**，现场已标定，不要动。

### 4.2 B_Vision (K230D)

- YOLO11 钢球检测 (`/data/marble_1.kmodel`)
- 触摸屏标定零轴
- 输出球相对零轴的 X 坐标 mm
- UART1 (GPIO40/41) 发给主控板

### 4.3 C_stepper (步进板)

- 接收主控板 UART 数据
- 双 PID (直道/弯道) + 前馈
- 步进电机 PWM 输出 (TIMG12)
- PID 参数全部为占位值, 现场标定

## 5. 硬件接线

### 主控板 ↔ 步进板

| 主控板 | 步进板 | 说明 |
|--------|--------|------|
| PA8 (UART1 TX) | PA10 (UART0 RX) | 数据 |
| GND | GND | 共地 |

### 其他引脚

详见 `A_Driver/pin_connections.md` 和 `C_stepper/README.md`。

## 6. 调试顺序

1. **K230 单独**: IDE 跑 `B_Vision/main.py`, 确认球能检出、X 坐标正常
2. **主控板单独**: CCS 编译 `A_Driver/empty.c`, 确认巡线+电机 OK
3. **主控+步进联调**: 接 UART, 主控发固定坐标, 看步进电机转不转
4. **全系统联调**: K230→主控→步进→摆杆, 上赛道调 PID
