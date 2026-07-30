# B_Vision

2026 年 TI 杯 **H 题：车载平衡滚球运动控制系统** 的视觉识别程序。

> 说明：仓库早期目录名 `medicine_car` 来自赛前练习题目“智能送药小车”，实际参赛题目为 H 题。本目录现仅保留 H 题比赛视觉代码。

## 1. 文件说明

| 文件 | 说明 |
| --- | --- |
| `main.py` | H 题主程序：触摸标定竖直零轴 + YOLO11 钢球识别 + UART 输出钢球 X 坐标 |

历史文件 `vision_line.py`（巡线开发版）已删除，不再使用。

## 2. 功能概述

1. **竖直零轴标定**：开机后通过触摸屏选择“钢球目标位置”对应的竖直线 `x=0`。
   - `default`：直接使用图像中心线作为零轴。
   - `new`：点击屏幕自定义零轴位置，支持 `ok` 保存、`delete` 重选。
   - 标定结果保存到 `/sdcard/axis_x.txt`，下次上电自动读取。
2. **钢球检测**：基于 YOLO11 目标检测模型 `marble_1.kmodel`，每帧识别画面中的钢球。
3. **坐标计算**：取置信度最高的钢球框，计算其中心到零轴的水平距离，并按 `PX_TO_MM` 换算为毫米。
4. **UART 输出**：将 X 坐标以固定协议发送给 MSPM0 主控。
5. **画面显示**：实时叠加钢球框、零轴线、X 坐标、置信度、帧率等信息。

## 3. 代码架构

```text
main.py
│
├─ 初始化
│   ├─ init_uart()              # UART1 初始化
│   ├─ TOUCH(0)                 # 触摸屏初始化
│   ├─ PipeLine                 # 摄像头 + OSD 显示通路
│   └─ YOLO11                   # 加载 marble_1.kmodel
│
├─ 交互状态机
│   ├─ MODE_MENU                # 选择 default / new
│   ├─ MODE_PICK                # 点击屏幕选择零轴
│   ├─ MODE_CONFIRM             # ok / delete 确认
│   └─ MODE_RUN                 # 正常运行，识别并输出
│
├─ 识别与计算
│   ├─ yolo.run(img)            # 执行 YOLO 推理
│   ├─ pick_best_marble()       # 取最高置信度目标
│   ├─ 计算 cx, cy, score
│   └─ x_mm = (cx - axis_x) * PX_TO_MM
│
└─ 输出
    ├─ uart_send_x()            # 发送 X 坐标
    ├─ 画面 OSD 绘制
    └─ 丢帧时沿用 last_x_mm
```

## 4. 关键配置参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `KMODEL_PATH` | `/data/marble_1.kmodel` | YOLO 模型路径 |
| `LABELS` | `{0: "marble"}` | 模型输出类别 |
| `MODEL_INPUT` | `[320, 320]` | 模型输入分辨率 |
| `RGB888P_SIZE` | `[640, 360]` | 摄像头输出分辨率 |
| `CONF_THRESH` | `0.50` | 置信度阈值，低于该值的检测框丢弃 |
| `NMS_THRESH` | `0.45` | NMS 非极大值抑制阈值 |
| `MAX_BOXES` | `10` | 单帧最大检测框数量 |
| `PX_TO_MM` | `0.5` | 像素到毫米的换算比例 |
| `SAVE_PATH` | `/sdcard/axis_x.txt` | 零轴位置保存文件 |
| `UART_TX` / `UART_RX` | `40` / `41` | K230D UART1 引脚 |
| `UART_BAUD` | `115200` | UART 波特率 |

## 5. UART 通信协议

视觉模块向 MSPM0 发送钢球相对零轴的 X 坐标，帧格式固定为 6 字节：

```text
[0xAA][0x01][DH][DL][CHECKSUM][0x55]

CHECKSUM = (0x01 + DH + DL) & 0xFF
DH = (x_mm >> 8) & 0xFF
DL = x_mm & 0xFF
```

- `x_mm` 为 16 位有符号整数，单位毫米。
- 钢球在零轴右侧时 `x_mm` 为正，左侧时为负。
- 当前程序每 2 帧发送一次；未检测到钢球时发送上一次有效坐标 `last_x_mm`。

## 6. 运行流程

1. 将 `marble_1.kmodel` 放到 K230D 的 `/data/` 目录。
2. 将 `main.py` 复制到 K230D 的 `/sdcard/main.py`。
3. 上电启动后进入菜单：
   - 点击 **default**：以屏幕中心为零轴，直接进入识别。
   - 点击 **new**：点击屏幕选择零轴位置 → 点击 **ok** 保存，或 **delete** 重新选择。
4. 进入运行界面后，程序实时检测钢球并通过 UART 发送 X 坐标。
5. 点击左上角区域或右上角 `x` 按钮可返回菜单重新标定。

## 7. 硬件接线

```text
K230D GPIO40 (UART1 TX)  ->  MSPM0 PA9 (UART RX)
K230D GPIO41 (UART1 RX)  ->  MSPM0 PA8 (UART TX)
K230D GND                ->  MSPM0 GND
```

- 波特率：`115200`
- 数据位：8，无校验，1 位停止位

## 8. 依赖与目录

- 模型文件：`/data/marble_1.kmodel`
- 标定文件：`/sdcard/axis_x.txt`（首次运行后自动生成）
- 依赖库：`libs.PipeLine`、`libs.YOLO`、`libs.Utils`、`media.sensor`、`media.display`、`media.media`

## 9. 常见问题

1. **上电后没有检测框**：检查 `/data/marble_1.kmodel` 是否存在，以及 `RGB888P_SIZE` 与模型训练时使用的分辨率是否一致。
2. **坐标偏差大**：重新进入 `new` 模式标定零轴，并确认 `PX_TO_MM` 与实际摄像头安装距离匹配。
3. **MSPM0 收不到数据**：确认 UART 接线方向、波特率 `115200`，以及帧头 `0xAA`、帧尾 `0x55`、校验和解析是否正确。
4. **触摸屏无响应**：确认 K230D LCD 模组已正确连接，且系统触摸屏驱动正常。
