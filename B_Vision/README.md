# 视觉模块 (B_Vision)

## 文件说明

| 文件 | 用途 |
|------|------|
| `vision_line.py` | **⭐ 主程序**：快通道巡线+轨道类型(CV) + 慢通道数字识别(KPU) + UART通信 |
| `backup_vision.py` | 备份：双识别器版本（无巡线功能） |
| `yolo_marble.py` | 钢珠检测 |
| `yolo_number.py` | YOLO数字检测 |
| `clean_features.py` | 删除特征文件，重新训练前运行 |

## 快速开始

## 快速开始

1. 首次/重训：IDE 运行 `clean_features.py` → `vision_line.py`，依次展示数字 1~8 完成训练
2. 已有特征：直接运行 `vision_line.py`（特征保存在 `/sdcard/features_digit/`）

### 比赛部署（脱离 IDE）

1. 确认特征文件就绪（`/sdcard/features_digit/` 有 24 个 `.bin`）
2. 把 `vision_line.py` 拷贝到 K230 的 `/sdcard/` 并改名 `main.py`
3. 上电自动运行，无需 IDE
4. 开发调试时删除 `main.py` 即可恢复 IDE 连接

## 硬件接线

```
K230 GPIO40 (UART1 TX) → MSPM0 PA8 (UART0 RX)
K230 GPIO41 (UART1 RX) → MSPM0 PA9 (UART0 TX)
K230 GND                → MSPM0 GND
```

## UART 协议

```
帧格式: [0xAA][CMD][DH][DL][CHECKSUM][0x55]
CMD 0x01 = 巡线偏差 (int16, -160~+160)
CMD 0x02 = 数字识别 (1~8)
CMD 0x03 = 视觉状态 (0=OK, 2=丢线)
CMD 0x04 = 轨道类型 (straight=0, left=-80, right=80)
CHECKSUM = (CMD + DH + DL) & 0xFF
```

## 架构

```
快通道(每帧):
  自适应阈值 → 5ROI加权质心 → 偏转角 → UART 0x01 (15Hz)
              线性回归斜率 → 轨道类型 → UART 0x04

慢通道(每10帧):
  KPU recognition.kmodel → 数字识别(1~8) → UART 0x02

显示:
  LCD硬件直通摄像头 + OSD叠加(识别结果+巡线状态)
```
