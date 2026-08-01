"""
H题钢球坐标主程序 (精简版)

上电直接进入零轴微调界面, 简化触摸逻辑:
- 触摸每 2 帧轮询一次, 降低 read(1) 阻塞影响
- 按下沿只取 EVENT_DOWN, 不做复杂状态机
- 删除 new/menu/pick/confirm 多模式

模式:
- ADJUST: -3/+3 微调零轴, save & run 保存并进入识别
- RUN:    YOLO 钢球识别 + UART 输出, 点 x 返回 ADJUST
"""
from libs.PipeLine import PipeLine
from libs.YOLO import YOLO11
from libs.Utils import *
from media.sensor import *
from media.display import *
from media.media import *
from machine import UART, FPIOA, TOUCH
import image, os, gc, time


KMODEL_PATH = "/data/marble_1.kmodel"
LABELS = {0: "marble"}
MODEL_INPUT = [320, 320]
RGB888P_SIZE = [640, 360]
CONF_THRESH = 0.50
NMS_THRESH = 0.45
MAX_BOXES = 10

try:
    with open("/sdcard/px_to_mm.txt", "r") as _f:
        PX_TO_MM = float(_f.read().strip())
    print("[INIT] px_to_mm=%.3f (from file)" % PX_TO_MM)
except Exception:
    PX_TO_MM = 0.5
    print("[INIT] px_to_mm=%.3f (default)" % PX_TO_MM)

SAVE_PATH = "/sdcard/axis_x.txt"
# SAVE_PATH_NEW = "/sdcard/axis_x_new.txt"  # 已删除 new 功能

UART_TX = 40
UART_RX = 41
UART_BAUD = 115200

MODE_ADJUST = 0
MODE_RUN = 1
# MODE_MENU   = 2  # 已删除, 上电直接进 ADJUST
# MODE_PICK   = 3  # 已删除 new 功能
# MODE_CONFIRM= 4  # 已删除 new 功能


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def save_axis_x(path, axis_x):
    try:
        with open(path, "w") as f:
            f.write(str(int(axis_x)))
        print("[AXIS] saved x0=%d to %s" % (int(axis_x), path))
    except Exception as e:
        print("[SAVE_FAIL] %s" % str(e))


def read_saved_axis(path, default_x):
    try:
        with open(path, "r") as f:
            return int(f.read().strip())
    except Exception:
        return default_x


def read_touch(dev):
    """读取触摸, 返回 (x, y, is_down) 或 None。

    只读 1 个事件, FIFO 空时立即返回 None, 不阻塞。
    is_down: 当前帧 EVENT_DOWN 或 EVENT_MOVE (手指在屏上)
    """
    try:
        one = dev.read(1)
        if not one:
            return None
        p = one[0]
        pressed = (p.event == TOUCH.EVENT_DOWN or p.event == TOUCH.EVENT_MOVE)
        return int(p.x), int(p.y), pressed
    except Exception as e:
        print("[TOUCH_READ_FAIL] %s" % str(e))
    return None


def in_rect(x, y, rect):
    rx, ry, rw, rh = rect
    return x >= rx and x <= rx + rw and y >= ry and y <= ry + rh


def draw_rect(img, rect, color, thickness=2):
    x, y, w, h = rect
    img.draw_line(x, y, x + w, y, color=color, thickness=thickness)
    img.draw_line(x, y + h, x + w, y + h, color=color, thickness=thickness)
    img.draw_line(x, y, x, y + h, color=color, thickness=thickness)
    img.draw_line(x + w, y, x + w, y + h, color=color, thickness=thickness)


def draw_button(img, rect, text, color):
    draw_rect(img, rect, color, 2)
    img.draw_string_advanced(rect[0] + 22, rect[1] + 22, 28, text, color=color)


def pick_best_marble(result):
    try:
        boxes = result[0]
        cls_ids = result[1]
        scores = result[2]
        n = min(len(boxes), len(cls_ids), len(scores))
        if n <= 0:
            return None, result
        best = 0
        for i in range(1, n):
            if float(scores[i]) > float(scores[best]):
                best = i
        box = boxes[best]
        x, y, w, h = int(box[0]), int(box[1]), int(box[2]), int(box[3])
        score = float(scores[best])
        cls_id = int(cls_ids[best])
        det = {
            "x": x, "y": y, "w": w, "h": h,
            "cx": x + w // 2, "cy": y + h // 2,
            "score": score, "cls_id": cls_id
        }
        return det, ([box], [cls_id], [scores[best]])
    except Exception as e:
        print("[PICK_FAIL] %s" % str(e))
        return None, result


def uart_send_x(u, x_mm):
    val = int(x_mm) & 0xFFFF
    dh = (val >> 8) & 0xFF
    dl = val & 0xFF
    cs = (0x01 + dh + dl) & 0xFF
    u.write(bytes([0xAA, 0x01, dh, dl, cs, 0x55]))


def init_uart():
    fpioa = FPIOA()
    fpioa.set_function(UART_TX, FPIOA.UART1_TXD, ie=1, oe=1)
    fpioa.set_function(UART_RX, FPIOA.UART1_RXD, ie=1, oe=1)
    try:
        uart = UART(UART.UART1, baudrate=UART_BAUD)
        print("[INIT] UART1 OK")
        return uart
    except Exception as e:
        print("[INIT] UART1 FAIL: %s" % str(e))
        return None


# ==================== 初始化 ====================
uart = init_uart()
touch = TOUCH(0)

pl = PipeLine(rgb888p_size=RGB888P_SIZE, display_mode="lcd")
pl.create()
ds = pl.get_display_size()
display_w, display_h = ds[0], ds[1]

yolo = YOLO11(
    task_type="detect", mode="video",
    kmodel_path=KMODEL_PATH, labels=LABELS,
    rgb888p_size=RGB888P_SIZE, model_input_size=MODEL_INPUT,
    display_size=ds, conf_thresh=CONF_THRESH,
    nms_thresh=NMS_THRESH, max_boxes_num=MAX_BOXES,
    debug_mode=0
)
yolo.config_preprocess()

# NPU 首帧预热
print("[INIT] NPU warm-up start...")
_warmup_t0 = time.ticks_ms()
try:
    _warmup_img = pl.get_frame()
    if _warmup_img:
        _ = yolo.run(_warmup_img)
        gc.collect()
    print("[INIT] NPU warm-up done (%dms)" % time.ticks_diff(time.ticks_ms(), _warmup_t0))
except Exception as e:
    print("[INIT] NPU warm-up FAIL: %s" % str(e))

# 丢弃上电瞬间的触摸噪声
for _ in range(5):
    try:
        touch.read(1)
    except Exception:
        pass

# ==================== 按钮布局 ====================
adj_l1_btn  = (display_w // 2 - 140, display_h // 2 + 20, 100, 70)
adj_r1_btn  = (display_w // 2 + 40,  display_h // 2 + 20, 100, 70)
save_btn    = (display_w // 2 - 120, display_h - 130, 240, 80)
exit_btn    = (display_w - 80, 10, 60, 60)

# 已删除的按钮 (new 功能)
# default_btn = (display_w // 2 - 250, display_h // 2 - 70, 210, 90)
# new_btn     = (display_w // 2 + 40, display_h // 2 - 70, 210, 90)
# ok_btn      = (display_w // 2 - 250, display_h - 120, 210, 80)
# delete_btn  = (display_w // 2 + 40, display_h - 120, 210, 80)

# ==================== 状态变量 ====================
mode = MODE_ADJUST  # 上电直接进微调界面
axis_x = clamp(read_saved_axis(SAVE_PATH, display_w // 2), 0, display_w - 1)
last_x_mm = 0

# 简化触摸: 只记上一帧是否按下 + 当前帧坐标
touch_was_down = False
touch_x, touch_y = 0, 0

frame = 0
clock = time.clock()

print("[RUN] boot to ADJUST, axis_x=%d" % axis_x)

while True:
    os.exitpoint()
    clock.tick()
    frame += 1

    img = pl.get_frame()

    # ---- 触摸轮询 (每 2 帧读一次, 降低 read(1) 潜在阻塞的影响) ----
    touch_just_pressed = False
    if frame % 2 == 0:
        tp = read_touch(touch)
        if tp:
            tx, ty, down = tp
            touch_x, touch_y = tx, ty
            if down and not touch_was_down:
                touch_just_pressed = True
            touch_was_down = down
        else:
            touch_was_down = False

    pl.osd_img.clear()

    # ==================== ADJUST 模式 ====================
    if mode == MODE_ADJUST:
        pl.osd_img.draw_line(axis_x, 0, axis_x, display_h - 1,
                             color=(0, 255, 0), thickness=2)
        draw_button(pl.osd_img, adj_l1_btn, "-3", (0, 200, 255))
        draw_button(pl.osd_img, adj_r1_btn, "+3", (0, 200, 255))
        draw_button(pl.osd_img, save_btn, "save & run", (0, 255, 0))
        pl.osd_img.draw_string_advanced(20, 20, 24,
                                        "x0=%d  tap -3/+3  hold=fast" % axis_x,
                                        color=(255, 255, 255))

        if touch_just_pressed:
            if in_rect(touch_x, touch_y, adj_l1_btn):
                axis_x = clamp(axis_x - 3, 0, display_w - 1)
            elif in_rect(touch_x, touch_y, adj_r1_btn):
                axis_x = clamp(axis_x + 3, 0, display_w - 1)
            elif in_rect(touch_x, touch_y, save_btn):
                save_axis_x(SAVE_PATH, axis_x)
                mode = MODE_RUN

        # 按住不放时连续微调 (每 5 帧触发一次)
        if touch_was_down:
            if in_rect(touch_x, touch_y, adj_l1_btn) and frame % 5 == 0:
                axis_x = clamp(axis_x - 3, 0, display_w - 1)
            elif in_rect(touch_x, touch_y, adj_r1_btn) and frame % 5 == 0:
                axis_x = clamp(axis_x + 3, 0, display_w - 1)

    # ==================== RUN 模式 ====================
    elif mode == MODE_RUN:
        try:
            res = yolo.run(img)
        except Exception as e:
            print("[YOLO_CRASH] %s, skip frame" % str(e))
            res = ([], [], [])
        det, best_res = pick_best_marble(res)
        yolo.draw_result(best_res, pl.osd_img)
        pl.osd_img.draw_line(axis_x, 0, axis_x, display_h - 1,
                             color=(0, 255, 0), thickness=2)
        draw_button(pl.osd_img, exit_btn, "x", (255, 0, 0))

        if touch_just_pressed:
            if touch_x < 120 and touch_y < 80 or in_rect(touch_x, touch_y, exit_btn):
                mode = MODE_ADJUST

        if det:
            cx = int(det["cx"])
            cy = int(det["cy"])
            br = max(2, int(min(det["w"], det["h"]) * 0.48))
            x_mm = int((cx - axis_x) * PX_TO_MM)
            last_x_mm = x_mm

            pl.osd_img.draw_circle(cx, cy, br, color=(0, 255, 0), thickness=2)
            pl.osd_img.draw_line(cx, cy, axis_x, cy, color=(255, 255, 0), thickness=1)
            pl.osd_img.draw_string_advanced(
                10, 10, 22,
                "X:%+dmm PX:%d X0:%d C:%d FPS:%.1f" %
                (x_mm, cx, axis_x, int(det["score"] * 100), clock.fps()),
                color=(0, 255, 0)
            )
            if uart and frame % 2 == 0:
                uart_send_x(uart, x_mm)
        else:
            pl.osd_img.draw_string_advanced(
                10, 10, 22,
                "No ball last:%+dmm X0:%d FPS:%.1f" % (last_x_mm, axis_x, clock.fps()),
                color=(255, 0, 0)
            )
            if uart and frame % 2 == 0:
                uart_send_x(uart, last_x_mm)

    pl.show_image()
    if frame % 15 == 0:
        gc.collect()
