# C_stepper — 步进电机控球板 (MSPM0G3507)

串级双 PID + 编码器闭环 + 步进 Burst 控制的球平衡系统。

## 文件

| 文件 | 用途 |
|------|------|
| `main.c` | 完整固件：串级双PID + 编码器 + K230协议解析 + UART调参命令 |
| `empty.syscfg` | SysConfig 配置文件 |
| `ti_msp_dl_config.c/h` | DriverLib 自动生成的硬件初始化 |

## 硬件接线

| 信号 | 引脚 | 说明 |
|------|------|------|
| STEP | PA17 / TIMA1_CCP0 | 步进脉冲 (D36A驱动器) |
| DIR | PA13 | 步进方向 |
| EN | PA12 | 驱动器使能 (高有效) |
| ENC_A | PA0 | 编码器 A 相 |
| ENC_B | PA1 | 编码器 B 相 |
| ENC_Z | PA26 | 编码器 Z 相 (归零) |
| DEBUG TX | PA10 | UART0 → PC (printf + 调参) |
| DEBUG RX | PA11 | UART0 ← PC (调参命令) |
| K230 RX | PB7 | UART1 ← K230 (球坐标) |
| K230 TX | PB6 | UART1 → K230 (预留) |

## 控制架构

```
球 X 坐标 (K230 UART)
    │
    ▼
┌──────────────────────┐
│ 外层 Balance PID     │  5ms 周期
│ 输入: 球 X 误差 mm   │  KP/KI/KD → 目标倾角 (deg)
│ 输出: 目标倾角 deg   │  max ±4°
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│ 内层 Encoder PID     │  5ms 周期
│ 输入: 倾角误差 deg   │  KP/KI/KD → 控制量 (deg)
│ 输出: 步进位移 deg   │  max ±2°
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│ Burst 步进控制       │
│ 频率 5档: 400~2200Hz │  max 8 步/tick
│ 方向 + 脉冲 → D36A   │  16 微步细分
└──────────────────────┘
       │
       ▼
    编码器反馈 (4096线, 归零)
```

## 通信协议

### K230 → 步进板 (UART1, 115200 8N1)

```
AA 01 DH DL CS 55
  DH:DL = ball_x_mm (int16, big-endian, 左负右正)
  CS = (0x01 + DH + DL) & 0xFF
```

### PC ↔ 步进板 (UART0, DEBUG, 115200 8N1)

文本命令行接口，每行以 `\r` 或 `\n` 结尾：

| 命令 | 说明 | 示例 |
|------|------|------|
| `kp_b 0.06` | 设置外层 KP | 值越大响应越强 |
| `ki_b 0.001` | 设置外层 KI | 消除静差 |
| `kd_b 0.0002` | 设置外层 KD | 抑制震荡 |
| `kp_e 1.5` | 设置内层 KP | 编码器角度跟踪 |
| `ki_e 0.1` | 设置内层 KI | |
| `kd_e 0.03` | 设置内层 KD | |
| `tilt_max 4.0` | 最大倾角限制 (deg) | |
| `enc_max 2.0` | 内环最大输出 (deg) | |
| `freq <0-4> <Hz>` | 设置频率档位 | `freq 2 1500` |
| `burst 8` | 每 tick 最大步数 | 防止丢步 |
| `deadband 2` | 视觉死区 (mm) | |
| `timeout 200` | 丢球超时 (ms) | |
| `status` | 打印所有参数 | |
| `reset` | 清零 PID + 编码器归位 | |
| `self_test` | 运行自检 | |
| `help` | 命令列表 | |

## UART 调参流程

### 准备
1. PC 用 USB-UART 模块连接步进板 **DEBUG UART** (PA10/PA11)
2. K230 连接步进板 **VISION UART** (PB6/PB7)，或使用 Python 工具的 `--sim` 模式
3. 烧录最新固件，观察上电自检（电机正转44步 → 反转44步）

### Step 1: 开环验证
用 Python 工具确认电机方向和编码器方向正确。
观察 printf 输出 `[BAL] t=... enc=... deg` — 手动转动摆杆，编码器读数应跟随变化。

### Step 2: 内环编码器 PID 调参（先断开外环）
将 `kp_b=0, ki_b=0, kd_b=0` 关闭外环，然后用调试命令直接设目标倾角。
1. `kp_e 0.5` → 逐步增大，直到角度跟踪快但有轻微超调
2. `kd_e 0.02` → 抑制震荡
3. `ki_e 0.05` → 消除静差
4. 目标：编码器角度能稳定跟踪 `target` 值

### Step 3: 外环 Balance PID 调参
1. `kp_b 0.01` → 放球偏离中心 ~30mm
2. 逐步增大 `kp_b`，直到球能返回中心（临界震荡时记录 Ku, Tu）
3. `kd_b 0.0001` → 抑制超调
4. `ki_b 0.0005` → 消除稳态偏差
5. 目标：球偏离 50mm 后能在 1-2 秒内回中心 ±10mm

### Step 4: 频率档位优化
1. 观察大误差时的响应速度 → 调高 `freq 4` (far)
2. 观察小误差时有无过冲 → 调低 `freq 1` (fine)
3. `burst 8` 可适当增大到 12-16 如果电机力矩足够

### Step 5: 全链路联调
- 恢复外环参数
- K230 接入，上赛道跑球
- 弯道时观察球偏移 → 可暂时不做前馈，靠 PID 硬抗

## Python 调参工具

```bash
cd tools/
pip install pyserial          # 必需
pip install matplotlib        # 可选（实时绘图）

# 交互模式
python stepper_tuner.py COM3

# 带模拟球信号 (不需要K230也能调参)
python stepper_tuner.py COM3 --sim

# 记录数据
python stepper_tuner.py COM3 --sim --log test_run1.csv

# 发单条命令
python stepper_tuner.py COM3 --send "status"
```

## 默认参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| g_bal_kp | 0.055 | 外层 P |
| g_bal_ki | 0.0008 | 外层 I |
| g_bal_kd | 0.00018 | 外层 D |
| g_enc_kp | 1.25 | 内层 P |
| g_enc_ki | 0.10 | 内层 I |
| g_enc_kd | 0.03 | 内层 D |
| g_freq_tiers | 400/700/1100/1600/2200 Hz | 五档频率 |
| g_burst_max_steps | 8 | 每 tick 最大步数 |
| g_vision_timeout_ms | 200 | 丢球超时 |
