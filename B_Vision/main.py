"""
H题钢球坐标主程序: 触摸选择竖直零轴 + YOLO 识别钢球

启动菜单:
- default: 使用屏幕中心竖直线作为 x=0, 直接进入识别
- new: 触摸选择竖直线位置, 再 ok/delete 确认

主程序:
- 只取最高置信度钢球框
- 显示相对竖直零轴的 X 坐标
- UART 输出 X(mm): AA 01 DH DL CS 55
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

PX_TO_MM = 0.5
SAVE_PATH = "/sdcard/axis_x.txt"

UART_TX = 40
UART_RX = 41
UART_BAUD = 115200


MODE_MENU = 0
MODE_PICK = 1
MODE_CONFIRM = 2
MODE_RUN = 3


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def save_axis_x(axis_x):
    try:
        with open(SAVE_PATH, "w") as f:
            f.write(str(int(axis_x)))
        print("[AXIS] saved x0=%d" % int(axis_x))
    except Exception as e:
        print("[SAVE_FAIL] %s" % str(e))


def read_saved_axis(default_x):
    try:
        with open(SAVE_PATH, "r") as f:
            return int(f.read().strip())
    except Exception:
        return default_x


def read_touch(dev):
    try:
        points = dev.read(1)
        if len(points):
            p = points[0]
            pressed = p.event == TOUCH.EVENT_DOWN or p.event == TOUCH.EVENT_MOVE
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
        x = int(box[0])
        y = int(box[1])
        w = int(box[2])
        h = int(box[3])
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


uart = init_uart()
touch = TOUCH(0)

pl = PipeLine(rgb888p_size=RGB888P_SIZE, display_mode="lcd")
pl.create()
ds = pl.get_display_size()
display_w, display_h = ds[0], ds[1]

yolo = YOLO11(
    task_type="detect",
    mode="video",
    kmodel_path=KMODEL_PATH,
    labels=LABELS,
    rgb888p_size=RGB888P_SIZE,
    model_input_size=MODEL_INPUT,
    display_size=ds,
    conf_thresh=CONF_THRESH,
    nms_thresh=NMS_THRESH,
    max_boxes_num=MAX_BOXES,
    debug_mode=0
)
yolo.config_preprocess()

default_btn = (display_w // 2 - 250, display_h // 2 - 70, 210, 90)
new_btn = (display_w // 2 + 40, display_h // 2 - 70, 210, 90)
ok_btn = (display_w // 2 - 250, display_h - 120, 210, 80)
delete_btn = (display_w // 2 + 40, display_h - 120, 210, 80)
exit_btn = (display_w - 80, 10, 60, 60)

mode = MODE_MENU
axis_x = clamp(read_saved_axis(display_w // 2), 0, display_w - 1)
selected_axis_x = axis_x
last_x_mm = 0
touch_was_down = False
ignore_until_release = False
frame = 0
clock = time.clock()

print("[RUN] marble touch axis main")

while True:
    os.exitpoint()
    clock.tick()
    frame += 1

    img = pl.get_frame()
    tp = read_touch(touch)
    touch_down = False
    touch_x, touch_y = 0, 0
    touch_pressed_edge = False
    if tp:
        touch_x, touch_y, touch_down = tp
        if ignore_until_release:
            if not touch_down:
                ignore_until_release = False
        else:
            touch_pressed_edge = touch_down and not touch_was_down
    touch_was_down = touch_down

    pl.osd_img.clear()

    if mode == MODE_MENU:
        draw_button(pl.osd_img, default_btn, "default", (0, 255, 0))
        draw_button(pl.osd_img, new_btn, "new", (255, 255, 0))
        pl.osd_img.draw_string_advanced(20, 20, 24, "Select x=0 vertical axis", color=(255, 255, 255))

        if touch_pressed_edge:
            if in_rect(touch_x, touch_y, default_btn):
                axis_x = display_w // 2
                mode = MODE_RUN
            elif in_rect(touch_x, touch_y, new_btn):
                selected_axis_x = display_w // 2
                mode = MODE_PICK

    elif mode == MODE_PICK:
        if touch_pressed_edge and not in_rect(touch_x, touch_y, ok_btn) and not in_rect(touch_x, touch_y, delete_btn):
            selected_axis_x = clamp(touch_x, 0, display_w - 1)
            mode = MODE_CONFIRM

        pl.osd_img.draw_line(selected_axis_x, 0, selected_axis_x, display_h - 1,
                             color=(0, 255, 0), thickness=2)
        pl.osd_img.draw_string_advanced(
            20, 20, 24,
            "Tap screen to set x=0",
            color=(255, 255, 255)
        )

    elif mode == MODE_CONFIRM:
        pl.osd_img.draw_line(selected_axis_x, 0, selected_axis_x, display_h - 1,
                             color=(0, 255, 0), thickness=2)
        draw_button(pl.osd_img, ok_btn, "ok", (0, 255, 0))
        draw_button(pl.osd_img, delete_btn, "delete", (255, 0, 0))
        pl.osd_img.draw_string_advanced(20, 20, 24, "x0=%d" % selected_axis_x, color=(255, 255, 255))

        if touch_pressed_edge:
            if in_rect(touch_x, touch_y, ok_btn):
                axis_x = selected_axis_x
                save_axis_x(axis_x)
                mode = MODE_RUN
                ignore_until_release = True
            elif in_rect(touch_x, touch_y, delete_btn):
                mode = MODE_PICK
                ignore_until_release = True

    elif mode == MODE_RUN:
        res = yolo.run(img)
        det, best_res = pick_best_marble(res)
        yolo.draw_result(best_res, pl.osd_img)
        pl.osd_img.draw_line(axis_x, 0, axis_x, display_h - 1, color=(0, 255, 0), thickness=2)
        draw_button(pl.osd_img, exit_btn, "x", (255, 0, 0))

        # Top-left touch or exit button goes back to menu for quick recalibration.
        if touch_pressed_edge and (touch_x < 120 and touch_y < 80 or in_rect(touch_x, touch_y, exit_btn)):
            mode = MODE_MENU

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
    if frame % 60 == 0:
        gc.collect()
