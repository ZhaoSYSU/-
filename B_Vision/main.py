import gc
import math
import os
import sys
import time

import image
from machine import FPIOA, UART
from media.display import *
from media.media import *
from media.sensor import *


# DNK230D / CanMV line-following project.
# Base initialization follows the ALIENTEK DNK230D CanMV camera/UART examples.
# The measurement model follows the Lingmou skill: low-cost scanline/ROI
# refinement every frame, lightweight validation, temporal smoothing, and UART.


# ------------------------- User configuration -------------------------

FAST_QVGA = False
DISPLAY_WIDTH = 320 if FAST_QVGA else 640
DISPLAY_HEIGHT = 240 if FAST_QVGA else 480
SENSOR_FPS = 90
TO_IDE = True

SENSOR_HMIRROR = False
SENSOR_VFLIP = False

DEBUG_OVERLAY = True
COLOR_PREVIEW = True
BINARY_PREVIEW = False
BINARY_PREVIEW_INVERT = True
PRINT_INTERVAL = 30
GC_INTERVAL = 180


# ------------------------- Line detector configuration -------------------------

# Black line threshold in grayscale. Raise the high value when black tape is
# seen as broken; lower it when shadows are detected as line.
BLACK_THRESHOLD = [(0, 115)]

# ROI format: x, y, w, h, weight. Bottom ROIs should usually be heavier.
if FAST_QVGA:
    ROIS = (
        (0, 202, 320, 28, 0.42),
        (0, 170, 320, 25, 0.28),
        (0, 137, 320, 23, 0.18),
        (0, 102, 320, 20, 0.08),
        (0, 65, 320, 18, 0.04),
    )
else:
    ROIS = (
        (0, 405, 640, 55, 0.42),
        (0, 340, 640, 50, 0.28),
        (0, 275, 640, 45, 0.18),
        (0, 205, 640, 40, 0.08),
        (0, 130, 640, 35, 0.04),
    )

MIN_BLOB_PIXELS = 18
MIN_BLOB_AREA = 24
MAX_ROI_CANDIDATES = 4
USE_SCANLINE_DETECTOR = True
SCAN_ROWS_PER_ROI = 5
MIN_LINE_WIDTH = int(DISPLAY_WIDTH * 0.025)
MAX_LINE_WIDTH = int(DISPLAY_WIDTH * 0.62)
ROW_THRESHOLD_RATIO = 0.45
MIN_ROW_CONTRAST = 18
MAX_SCAN_CENTER_JUMP = DISPLAY_WIDTH * 0.38
LINE_CENTER_X = DISPLAY_WIDTH // 2

EMA_ALPHA = 0.42
ANGLE_EMA_ALPHA = 0.35
DEVIATION_DEADBAND = 2
MAX_LOST_COAST_FRAMES = 4

TRACK_LEFT_VALUE = -80
TRACK_STRAIGHT_VALUE = 0
TRACK_RIGHT_VALUE = 80
TRACK_ANGLE_STRAIGHT_DEG = 5
TRACK_CONFIRM_FRAMES = 3
BOTTOM_ROI_REQUIRED = True
MAX_ROI_CENTER_JUMP = DISPLAY_WIDTH * 0.45

OVERLAY_DARK = (0, 0, 0)
OVERLAY_LIGHT = (255, 0, 0)
OVERLAY_POINT = (0, 255, 255)
OVERLAY_TEXT = (255, 255, 255)


# ------------------------- UART configuration -------------------------

UART_ENABLED = True
UART_ID = UART.UART1
UART_TX_PIN = 40
UART_RX_PIN = 41
UART_BAUDRATE = 115200
UART_SEND_EVERY_N_FRAMES = 2

# Protocol shared with A_Driver:
# [0xAA][CMD][DH][DL][CHECKSUM][0x55]
# CMD 0x01 = int16 line deviation, -320..+320
# CMD 0x03 = vision status, 0 OK, 2 line lost
# CMD 0x04 = track type, straight=0, left=-80, right=80


def clamp(value, low, high):
    if value < low:
        return low
    if value > high:
        return high
    return value


def pack_i16(value):
    value = int(clamp(value, -32768, 32767)) & 0xFFFF
    return (value >> 8) & 0xFF, value & 0xFF


def uart_send_i16(uart, command, value):
    high, low = pack_i16(value)
    checksum = (command + high + low) & 0xFF
    uart.write(bytes([0xAA, command, high, low, checksum, 0x55]))


def uart_send_u8(uart, command, value):
    value = int(value) & 0xFF
    checksum = (command + value) & 0xFF
    uart.write(bytes([0xAA, command, 0, value, checksum, 0x55]))


def init_uart():
    if not UART_ENABLED:
        return None
    fpioa = FPIOA()
    fpioa.set_function(UART_TX_PIN, FPIOA.UART1_TXD)
    fpioa.set_function(UART_RX_PIN, FPIOA.UART1_RXD)
    return UART(
        UART_ID,
        baudrate=UART_BAUDRATE,
        bits=UART.EIGHTBITS,
        parity=UART.PARITY_NONE,
        stop=UART.STOPBITS_ONE,
    )


class ConfirmFilter:
    def __init__(self, frames):
        self.frames = frames
        self.label = None
        self.count = 0
        self.locked = None

    def update(self, label):
        if label == self.label:
            self.count += 1
        else:
            self.label = label
            self.count = 1
        if self.count >= self.frames and self.locked != label:
            self.locked = label
            return label, True
        return self.locked, False


class LineTracker:
    def __init__(self):
        self.deviation = 0.0
        self.angle = 0.0
        self.lost_frames = MAX_LOST_COAST_FRAMES + 1
        self.track_filter = ConfirmFilter(TRACK_CONFIRM_FRAMES)

    def update(self, measurement):
        if measurement is None:
            self.lost_frames += 1
            if self.lost_frames <= MAX_LOST_COAST_FRAMES:
                return True
            return False

        raw_dev, raw_angle, raw_track, points = measurement
        if abs(raw_dev) <= DEVIATION_DEADBAND:
            raw_dev = 0

        if self.lost_frames > MAX_LOST_COAST_FRAMES:
            self.deviation = raw_dev
            self.angle = raw_angle
        else:
            self.deviation += EMA_ALPHA * (raw_dev - self.deviation)
            self.angle += ANGLE_EMA_ALPHA * (raw_angle - self.angle)
        self.lost_frames = 0
        self.track_filter.update(raw_track)

        return True

    def track_value(self):
        label = self.track_filter.locked
        if label == "left":
            return TRACK_LEFT_VALUE
        if label == "right":
            return TRACK_RIGHT_VALUE
        return TRACK_STRAIGHT_VALUE


def choose_blob(blobs):
    if not blobs:
        return None
    blobs.sort(key=lambda b: b.pixels(), reverse=True)
    limit = min(len(blobs), MAX_ROI_CANDIDATES)
    best = None
    best_score = -1
    for index in range(limit):
        blob = blobs[index]
        density = blob.density()
        if density < 0.035 or density > 0.95:
            continue
        score = blob.pixels() * (0.65 + density)
        if score > best_score:
            best = blob
            best_score = score
    return best


def pixel_gray(pixel):
    if isinstance(pixel, (tuple, list)):
        if len(pixel) >= 3:
            return (int(pixel[0]) * 30 + int(pixel[1]) * 59 + int(pixel[2]) * 11) // 100
        return int(pixel[0])
    value = int(pixel)
    if value <= 255:
        return value
    red = ((value >> 11) & 0x1F) * 255 // 31
    green = ((value >> 5) & 0x3F) * 255 // 63
    blue = (value & 0x1F) * 255 // 31
    return (red * 30 + green * 59 + blue * 11) // 100


def scanline_center(img, roi, last_x):
    x0, y0, width, height, weight = roi
    row_count = SCAN_ROWS_PER_ROI
    if row_count < 1:
        row_count = 1
    centers = []
    for row_index in range(row_count):
        y = y0 + (height * (row_index + 1)) // (row_count + 1)
        samples = []
        row_min = 255
        row_max = 0
        for x in range(x0, x0 + width):
            gray = pixel_gray(img.get_pixel(x, y))
            samples.append(gray)
            if gray < row_min:
                row_min = gray
            if gray > row_max:
                row_max = gray
        contrast = row_max - row_min
        if contrast < MIN_ROW_CONTRAST:
            continue
        threshold = row_min + int(contrast * ROW_THRESHOLD_RATIO)
        fixed_threshold = BLACK_THRESHOLD[0][1]
        if threshold > fixed_threshold:
            threshold = fixed_threshold

        in_run = False
        run_start = 0
        best_center = None
        best_score = -1000000
        for offset, gray in enumerate(samples):
            x = x0 + offset
            is_dark = gray <= threshold
            if is_dark and not in_run:
                run_start = x
                in_run = True
            if (not is_dark or offset == len(samples) - 1) and in_run:
                run_end = x - 1 if not is_dark else x
                run_width = run_end - run_start + 1
                if MIN_LINE_WIDTH <= run_width <= MAX_LINE_WIDTH:
                    center = (run_start + run_end) * 0.5
                    expected = LINE_CENTER_X if last_x is None else last_x
                    center_score = width - abs(center - expected)
                    width_score = width - abs(run_width - width * 0.25)
                    score = center_score * 2 + width_score
                    if score > best_score:
                        best_score = score
                        best_center = center
                in_run = False
        if best_center is not None:
            centers.append(best_center)
    if not centers:
        return None
    return sum(centers) / len(centers)


def detect_line_scanline(img):
    weighted_x = 0.0
    weight_sum = 0.0
    points = []
    last_x = None

    for index, roi in enumerate(ROIS):
        cx = scanline_center(img, roi, last_x)
        if cx is None:
            if index == 0 and BOTTOM_ROI_REQUIRED:
                return None
            continue
        if last_x is not None and abs(cx - last_x) > MAX_SCAN_CENTER_JUMP:
            continue
        cy = roi[1] + roi[3] // 2
        points.append((int(cx), cy, roi, None))
        weighted_x += cx * roi[4]
        weight_sum += roi[4]
        last_x = cx

    if weight_sum <= 0:
        return None

    center_x = weighted_x / weight_sum
    deviation = center_x - LINE_CENTER_X
    angle = -math.degrees(math.atan(deviation / (DISPLAY_WIDTH * 0.375)))

    track = "straight"
    if len(points) >= 3:
        points_for_slope = sorted(points, key=lambda item: item[1])
        top_x, top_y = points_for_slope[0][0], points_for_slope[0][1]
        bottom_x, bottom_y = points_for_slope[-1][0], points_for_slope[-1][1]
        dy = bottom_y - top_y
        if abs(dy) > 1:
            drift_angle = math.degrees(math.atan((bottom_x - top_x) / dy))
            if drift_angle > TRACK_ANGLE_STRAIGHT_DEG:
                track = "right"
            elif drift_angle < -TRACK_ANGLE_STRAIGHT_DEG:
                track = "left"

    return int(deviation), int(angle), track, points

def detect_line(img):
    if USE_SCANLINE_DETECTOR:
        return detect_line_scanline(img)
    weighted_x = 0.0
    weight_sum = 0.0
    points = []
    last_x = None

    for index, roi in enumerate(ROIS):
        blobs = img.find_blobs(
            BLACK_THRESHOLD,
            roi=roi[0:4],
            pixels_threshold=MIN_BLOB_PIXELS,
            area_threshold=MIN_BLOB_AREA,
            merge=True,
        )
        blob = choose_blob(blobs)
        if blob is None:
            if index == 0 and BOTTOM_ROI_REQUIRED:
                return None
            continue

        cx = blob.cx()
        cy = blob.cy()
        if last_x is not None and abs(cx - last_x) > MAX_ROI_CENTER_JUMP:
            continue

        points.append((cx, cy, roi, blob))
        weighted_x += cx * roi[4]
        weight_sum += roi[4]
        last_x = cx

    if weight_sum <= 0:
        return None

    center_x = weighted_x / weight_sum
    deviation = center_x - LINE_CENTER_X
    angle = -math.degrees(math.atan(deviation / (DISPLAY_WIDTH * 0.375)))

    track = "straight"
    if len(points) >= 3:
        # Use top-to-bottom center drift as a cheap curve/intersection hint.
        points_for_slope = sorted(points, key=lambda item: item[1])
        top_x, top_y = points_for_slope[0][0], points_for_slope[0][1]
        bottom_x, bottom_y = points_for_slope[-1][0], points_for_slope[-1][1]
        dy = bottom_y - top_y
        if abs(dy) > 1:
            drift_angle = math.degrees(math.atan((bottom_x - top_x) / dy))
            if drift_angle > TRACK_ANGLE_STRAIGHT_DEG:
                track = "right"
            elif drift_angle < -TRACK_ANGLE_STRAIGHT_DEG:
                track = "left"

    return int(deviation), int(angle), track, points


def draw_debug(img, measurement, tracker, fps_text):
    img.draw_string_advanced(0, 0, 20, fps_text, color=OVERLAY_TEXT, thickness=2)
    if measurement is None:
        img.draw_string_advanced(0, 24, 20, "LOST", color=OVERLAY_TEXT, thickness=2)
        return
    for point in measurement[3]:
        blob = point[3]
        if blob is not None:
            img.draw_rectangle([v for v in blob.rect()], color=OVERLAY_POINT, thickness=2)
            img.draw_cross(blob.cx(), blob.cy(), color=OVERLAY_POINT)
        else:
            roi = point[2]
            img.draw_rectangle(roi[0:4], color=OVERLAY_POINT, thickness=1)
            img.draw_cross(point[0], point[1], color=OVERLAY_POINT, size=8, thickness=2)
    guide_x = int(LINE_CENTER_X + tracker.deviation)
    guide = (LINE_CENTER_X, DISPLAY_HEIGHT, guide_x, 250)
    img.draw_line(guide, color=OVERLAY_DARK, thickness=8)
    img.draw_line(guide, color=OVERLAY_LIGHT, thickness=4)
    img.draw_cross(guide_x, 250, color=OVERLAY_LIGHT, size=12, thickness=3)
    text = "D:%+d A:%+d T:%d" % (
        int(tracker.deviation),
        int(tracker.angle),
        tracker.track_value(),
    )
    img.draw_string_advanced(0, 24, 20, text, color=OVERLAY_TEXT, thickness=2)


def init_sensor():
    sensor = Sensor(width=1280, height=960)
    sensor.reset()
    if COLOR_PREVIEW and not BINARY_PREVIEW:
        sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, chn=CAM_CHN_ID_0)
        sensor.set_pixformat(Sensor.YUV420SP, chn=CAM_CHN_ID_0)
        sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, chn=CAM_CHN_ID_2)
        sensor.set_pixformat(Sensor.GRAYSCALE, chn=CAM_CHN_ID_2)
    else:
        sensor.set_framesize(Sensor.QVGA if FAST_QVGA else Sensor.VGA)
        sensor.set_pixformat(Sensor.GRAYSCALE)
    if SENSOR_HMIRROR:
        sensor.set_hmirror(True)
    if SENSOR_VFLIP:
        sensor.set_vflip(True)
    return sensor


def init_display(sensor):
    if COLOR_PREVIEW and not BINARY_PREVIEW:
        Display.bind_layer(
            **sensor.bind_info(x=0, y=0, chn=CAM_CHN_ID_0),
            layer=Display.LAYER_VIDEO1
        )
    Display.init(
        Display.ST7701,
        width=DISPLAY_WIDTH,
        height=DISPLAY_HEIGHT,
        fps=SENSOR_FPS,
        to_ide=TO_IDE,
    )


def main():
    sensor = None
    uart = None
    display_ready = False
    media_ready = False
    try:
        sensor = init_sensor()
        init_display(sensor)
        display_ready = True
        MediaManager.init()
        media_ready = True
        osd = None
        if COLOR_PREVIEW and not BINARY_PREVIEW:
            osd = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
        try:
            uart = init_uart()
        except BaseException as error:
            print("UART disabled; vision continues")
            print(error)
            uart = None

        sensor.run()
        clock = time.clock()
        tracker = LineTracker()
        frame_id = 0
        fps_text = "FPS: --"

        print("DNK230D Lingmou line follower started")
        print("uart1=%s tx=%d rx=%d baud=%d" % (
            "ON" if uart is not None else "OFF",
            UART_TX_PIN,
            UART_RX_PIN,
            UART_BAUDRATE,
        ))

        while True:
            os.exitpoint()
            clock.tick()
            if COLOR_PREVIEW and not BINARY_PREVIEW:
                img = sensor.snapshot(chn=CAM_CHN_ID_2)
            else:
                img = sensor.snapshot()
            measurement = detect_line(img)
            line_ok = tracker.update(measurement)

            if frame_id % UART_SEND_EVERY_N_FRAMES == 0 and uart is not None:
                uart_send_i16(uart, 0x01, int(tracker.deviation))
                uart_send_u8(uart, 0x03, 0 if line_ok else 2)
                if line_ok:
                    uart_send_i16(uart, 0x04, tracker.track_value())

            if frame_id % 5 == 0:
                fps_text = "FPS: %.1f" % clock.fps()

            if COLOR_PREVIEW and not BINARY_PREVIEW:
                osd.clear()
                if DEBUG_OVERLAY:
                    draw_debug(osd, measurement, tracker, fps_text)
                else:
                    osd.draw_string_advanced(0, 0, 20, fps_text, color=OVERLAY_TEXT, thickness=2)
                Display.show_image(osd, 0, 0, Display.LAYER_OSD3)
            else:
                if BINARY_PREVIEW:
                    try:
                        img.binary(BLACK_THRESHOLD, invert=BINARY_PREVIEW_INVERT)
                    except TypeError:
                        img.binary(BLACK_THRESHOLD)
                        if BINARY_PREVIEW_INVERT:
                            img.invert()

                if DEBUG_OVERLAY:
                    draw_debug(img, measurement, tracker, fps_text)
                else:
                    img.draw_string_advanced(0, 0, 20, fps_text, color=OVERLAY_TEXT, thickness=2)

                Display.show_image(img)

            if frame_id % PRINT_INTERVAL == 0:
                print(
                    "FPS=%.1f ok=%d dev=%d angle=%d track=%d" % (
                        clock.fps(),
                        1 if line_ok else 0,
                        int(tracker.deviation),
                        int(tracker.angle),
                        tracker.track_value(),
                    )
                )

            frame_id += 1
            if frame_id % GC_INTERVAL == 0:
                gc.collect()

    except KeyboardInterrupt as error:
        print("user stop:", error)
    except BaseException as error:
        print(error)
    finally:
        if isinstance(sensor, Sensor):
            try:
                sensor.stop()
            except BaseException:
                pass
        if uart is not None:
            try:
                uart.deinit()
            except BaseException:
                pass
        if display_ready:
            Display.deinit()
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        time.sleep_ms(100)
        if media_ready:
            MediaManager.deinit()


if __name__ == "__main__":
    main()











