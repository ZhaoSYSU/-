# 视觉模块 (B_Vision)

## 文件说明

| 文件 | 用途 |
|------|------|
| `vision_line.py` | **⭐ 主程序**：快慢双通道（巡线+KPU数字/轨道识别+UART通信） |
| `vision_full.py` | 备份：双识别器版本（数字+轨道，无巡线功能） |
| `yolo_marble.py` | 钢珠检测（赛题潜在需求） |
| `yolo_number.py` | YOLO数字检测（需训练kmodel） |
| `clean_features.py` | 删除所有特征文件，重新训练前运行 |
| `mount_tf.py` | 外部TF卡挂载工具 |

## 快速开始

1. 首次或重新训练：IDE 运行 `clean_features.py` → `vision_line.py`
2. 已有特征文件：直接运行 `vision_line.py`
3. 训练完成后自动进入推理模式，K230 LCD 和 IDE 帧缓冲区同步显示

## 硬件接线

```
K230 UART1 TX (pin 3) → MSPM0 UART1 RX (PA9)
K230 UART1 RX (pin 4) → MSPM0 UART1 TX (PA8)
K230 GND               → MSPM0 GND
```

## UART 协议

```
帧格式: [0xAA][CMD][DH][DL][CHECKSUM][0x55]
CMD 0x01 = 巡线偏差 (int16, -160~+160)
CMD 0x02 = 数字识别 (1~8)
CMD 0x03 = 视觉状态 (0=OK, 2=丢线)
CMD 0x04 = 轨道类型 (0=直, -80=左, 80=右)
CHECKSUM = (CMD + DH + DL) & 0xFF
```

## 架构

```
快通道(每帧):
  numpy自适应阈值 → 多ROI加权质心 → 偏转角 → UART CMD 0x01 (15Hz)

慢通道(每10帧,交替):
  KPU recognition.kmodel → 数字识别 → UART CMD 0x02
  KPU recognition.kmodel → 轨道识别 → UART CMD 0x04
```
