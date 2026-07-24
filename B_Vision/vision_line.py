"""
智能送药小车 - 完整视觉程序
=============================
快通道(每帧): 灰度巡线 + 轨道类型判断 → UART
慢通道(每10帧): KPU数字识别 → UART

通信: UART2 (pin5=TX, pin6=RX) → MSPM0 UART1
协议: [0xAA][CMD][DH][DL][CS][0x55]
  CMD 0x01 = 巡线偏差 (int16, -160~+160)
  CMD 0x02 = 数字识别 (1~8)
  CMD 0x03 = 视觉状态 (0=OK, 2=丢线)
  CMD 0x04 = 轨道类型 (straight=0, left=-80, right=80)
"""
import os, gc, time, math
from media.display import *
from machine import UART, FPIOA
from libs.PipeLine import PipeLine
from libs.AIBase import AIBase
from libs.AI2D import Ai2d
from libs.Utils import *
import nncase_runtime as nn
import ulab.numpy as np

# ==================== 参数 ====================
DISPLAY_MODE = "lcd"
RGB888P_SIZE = [640, 360]
MODEL_PATH = "/sdcard/examples/kmodel/recognition.kmodel"
MODEL_INPUT = [224, 224]
FEATURE_DIM = 512

# 巡线
LINE_CENTER = 320
LINE_ADAPTIVE_RATIO = 0.4
LINE_MIN_CONTRAST = 30
LINE_MULTI = [
    (120, 20, 0.1),
    (170, 20, 0.1),
    (220, 20, 0.2),
    (270, 20, 0.3),
    (320, 25, 0.3),
]

# KPU数字识别
DIGIT_LABELS = ["1", "2", "3", "4", "5", "6", "7", "8"]
DIGIT_DIR = "/sdcard/features_digit/"
FEATURES_PER = 3
FRAMES_PER_FEATURE = 40
SIMILARITY_THRESHOLD = 0.65
KPU_INTERVAL = 10
TRAIN_PAUSE_FRAMES = 60

# UART
UART_TX_PIN = 40     # GPIO40 = UART1_TXD
UART_RX_PIN = 41     # GPIO41 = UART1_RXD
UART_BAUD = 115200
TRACK_VAL = {"straight": 0, "left": -80, "right": 80}


# ==================== 时序滤波 ====================
class TemporalFilter:
    """滑动窗口锁定机制，消除单帧跳变"""
    def __init__(self, window=8, lock_in=4, lock_out=6):
        self.history = []
        self.window = window
        self.lock_in = lock_in
        self.lock_out = lock_out
        self.locked_label = None
        self.unlock_count = 0

    def update(self, label):
        self.history.append(label)
        if len(self.history) > self.window:
            self.history.pop(0)
        if self.locked_label is None:
            recent = self.history[-self.lock_in:]
            if recent.count(recent[0]) == len(recent) and recent[0] is not None:
                self.locked_label = recent[0]
                return self.locked_label, True
        else:
            recent = self.history[-self.lock_out:]
            diff = [l for l in recent if l != self.locked_label]
            if len(diff) > len(recent) // 2:
                self.unlock_count += 1
                if self.unlock_count >= 2:
                    self.locked_label = None
                    self.unlock_count = 0
            else:
                self.unlock_count = 0
        return self.locked_label, False


# ==================== KPU特征识别器 ====================
class FeatureRecognizer(AIBase):
    """基于recognition.kmodel提取图像特征，余弦相似度匹配"""
    def __init__(self, name, labels, feature_dir, crop_x, crop_y,
                 crop_w, crop_h, box_color, rgb888p_size, display_size):
        super().__init__(MODEL_PATH, MODEL_INPUT, rgb888p_size, 0)
        self.name = name
        self.labels = labels
        self.feature_dir = feature_dir
        self.crop_x, self.crop_y = crop_x, crop_y
        self.crop_w, self.crop_h = crop_w, crop_h
        self.box_color = box_color
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.feature_db = {}
        self.trained = False
        self.ai2d = Ai2d(0)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT,
            nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)

    def config_preprocess(self):
        self.ai2d.crop(int(self.crop_x), int(self.crop_y),
                       int(self.crop_w), int(self.crop_h))
        self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
        self.ai2d.build(
            [1, 3, self.rgb888p_size[1], self.rgb888p_size[0]],
            [1, 3, MODEL_INPUT[1], MODEL_INPUT[0]])

    def postprocess(self, results):
        return results[0][0]

    def predict(self, vec):
        vec_norm = np.sqrt(np.sum(vec * vec))
        best_label, best_score = None, 0.0
        for label, feats in self.feature_db.items():
            for fvec in feats:
                sim = np.sum(vec * fvec) / (vec_norm * np.sqrt(np.sum(fvec * fvec)))
                if sim > best_score:
                    best_score = sim
                    best_label = label
        if best_score >= SIMILARITY_THRESHOLD:
            return best_label, best_score
        return None, best_score

    def save(self):
        try:
            os.mkdir(self.feature_dir)
        except:
            pass
        try:
            for fn in os.listdir(self.feature_dir):
                if fn.endswith('.bin'):
                    os.remove(self.feature_dir + fn)
        except:
            pass
        for label, feats in self.feature_db.items():
            for i, vec in enumerate(feats):
                with open(self.feature_dir + label + "_" + str(i) + '.bin', 'wb') as f:
                    f.write(vec.tobytes())
        print("[%s] Saved %d features" % (self.name,
            sum(len(v) for v in self.feature_db.values())))

    def load(self):
        try:
            files = os.listdir(self.feature_dir)
        except Exception as e:
            print("[%s] %s not found: %s" % (self.name, self.feature_dir, str(e)))
            return False
        loaded = 0
        for fn in files:
            if not fn.endswith('.bin'):
                continue
            try:
                label = fn.rsplit('_', 1)[0]
                with open(self.feature_dir + fn, 'rb') as f:
                    data = f.read()
                vec = np.frombuffer(data, dtype=np.float)
                if len(vec) != FEATURE_DIM:
                    print("[%s] Skip %s: dim=%d" % (self.name, fn, len(vec)))
                    continue
                if label not in self.feature_db:
                    self.feature_db[label] = []
                self.feature_db[label].append(vec)
                loaded += 1
            except Exception as e:
                print("[%s] Load %s failed: %s" % (self.name, fn, str(e)))
        if loaded > 0:
            self.trained = True
            print("[%s] Loaded %d features: %s" % (self.name, loaded,
                str(sorted(self.feature_db.keys()))))
            return True
        print("[%s] No features found" % self.name)
        return False

    def draw_box(self, pl, text):
        ox = int(self.crop_x * self.display_size[0] / self.rgb888p_size[0])
        oy = int(self.crop_y * self.display_size[1] / self.rgb888p_size[1])
        ow = int(self.crop_w * self.display_size[0] / self.rgb888p_size[0])
        oh = int(self.crop_h * self.display_size[1] / self.rgb888p_size[1])
        pl.osd_img.draw_rectangle(ox, oy, ow, oh,
                                   color=self.box_color, thickness=3)
        if text:
            pl.osd_img.draw_string_advanced(ox + 4, oy + 4, 18, text,
                                             color=self.box_color)

    def train(self, pl):
        print("\n[%s] Training: %s" % (self.name, str(self.labels)))
        for label_idx, label in enumerate(self.labels):
            for fi in range(FEATURES_PER):
                if fi > 0 or label_idx > 0:
                    for p in range(TRAIN_PAUSE_FRAMES):
                        os.exitpoint()
                        img = pl.get_frame()
                        pl.osd_img.clear()
                        self.draw_box(pl,
                            "%s: '%s' %d/%d  adjust target..." %
                            (self.name, label, fi + 1, FEATURES_PER))
                        pl.osd_img.draw_string_advanced(10, 10, 20,
                            "ADJUST then wait...", color=(255, 255, 0))
                        pl.show_image()
                tail_vecs = []
                for f in range(FRAMES_PER_FEATURE):
                    os.exitpoint()
                    img = pl.get_frame()
                    vec = self.run(img)
                    tail_vecs.append(vec)
                    if len(tail_vecs) > 5:
                        tail_vecs.pop(0)
                    pl.osd_img.clear()
                    self.draw_box(pl, "%s %s %d/%d f%d/%d" %
                        (self.name, label, fi + 1, FEATURES_PER,
                         f + 1, FRAMES_PER_FEATURE))
                    pl.show_image()
                avg_vec = np.mean(np.array(tail_vecs), axis=0)
                if label not in self.feature_db:
                    self.feature_db[label] = []
                self.feature_db[label].append(avg_vec)
        self.save()
        self.trained = True
        print("[%s] Training done!" % self.name)


# ==================== 巡线 + 轨道类型 ====================
def detect_line(frame_np):
    """多ROI自适应阈值巡线，线性回归判断轨道类型
    返回: (angle, deviation, track_type, line_ok)
    """
    s = frame_np.shape
    if len(s) == 4:
        h, w = s[2], s[3]
        gray = frame_np[0, 0]
    elif len(s) == 3:
        h, w = s[1], s[2]
        gray = frame_np[0]
    else:
        h, w = s[0], s[1]
        gray = frame_np

    centroids = []
    total_wx, total_wsum = 0.0, 0.0
    for y_off, roi_h, weight in LINE_MULTI:
        y_s, y_e = y_off, min(y_off + roi_h, h)
        if y_s >= h:
            continue
        strip = gray[y_s:y_e, :]
        vmin, vmax = np.min(strip), np.max(strip)
        contrast = vmax - vmin
        if contrast < LINE_MIN_CONTRAST:
            continue
        thresh = vmin + contrast * LINE_ADAPTIVE_RATIO
        dark = strip < thresh
        dark_cols = np.sum(dark, axis=0)
        s_dark = np.sum(dark_cols)
        if s_dark > 10:
            cx = np.sum(np.arange(w) * dark_cols) / s_dark
            mid_y = y_s + roi_h // 2
            centroids.append((mid_y, cx))
            total_wx += cx * weight
            total_wsum += weight

    if total_wsum == 0:
        return 0, 0, None, False

    center_x = total_wx / total_wsum
    deviation = center_x - LINE_CENTER
    angle = -math.atan(deviation / 80.0)
    angle = math.degrees(angle)

    # 线性回归斜率 → 轨道类型
    track_type = None
    if len(centroids) >= 3:
        ys = np.array([c[0] for c in centroids], dtype=np.float)
        xs = np.array([c[1] for c in centroids], dtype=np.float)
        n = len(ys)
        slope = (n * np.sum(ys * xs) - np.sum(ys) * np.sum(xs)) / \
                (n * np.sum(ys * ys) - np.sum(ys) ** 2)
        track_angle = math.degrees(math.atan(slope))
        if abs(track_angle) < 3:
            track_type = "straight"
        elif track_angle > 0:
            track_type = "right"
        else:
            track_type = "left"

    return int(angle), int(deviation), track_type, True


# ==================== UART ====================
def uart_send_deviation(u, deviation):
    val = deviation & 0xFFFF
    dh, dl = (val >> 8) & 0xFF, val & 0xFF
    cs = (0x01 + dh + dl) & 0xFF
    u.write(bytes([0xAA, 0x01, dh, dl, cs, 0x55]))

def uart_send_digit(u, digit):
    d = int(digit) & 0xFF
    cs = (0x02 + 0 + d) & 0xFF
    u.write(bytes([0xAA, 0x02, 0, d, cs, 0x55]))

def uart_send_status(u, status):
    cs = (0x03 + 0 + status) & 0xFF
    u.write(bytes([0xAA, 0x03, 0, status, cs, 0x55]))

def uart_send_track(u, label):
    v = TRACK_VAL.get(label, 0) & 0xFFFF
    dh, dl = (v >> 8) & 0xFF, v & 0xFF
    cs = (0x04 + dh + dl) & 0xFF
    u.write(bytes([0xAA, 0x04, dh, dl, cs, 0x55]))


# ==================== 主流程 ====================
print("[INIT] PipeLine...")
pl = PipeLine(rgb888p_size=RGB888P_SIZE, display_mode=DISPLAY_MODE)
pl.create()
ds = pl.get_display_size()

print("[INIT] UART1 TX=GPIO%d RX=GPIO%d" % (UART_TX_PIN, UART_RX_PIN))
fpioa = FPIOA()
fpioa.set_function(UART_TX_PIN, FPIOA.UART1_TXD, ie=1, oe=1)
fpioa.set_function(UART_RX_PIN, FPIOA.UART1_RXD, ie=1, oe=1)

uart_ok = True
try:
    uart = UART(UART.UART1, baudrate=UART_BAUD)
    print("[INIT] UART1 OK")
except Exception as e:
    uart = None
    uart_ok = False
    print("[INIT] UART1 FAILED: %s" % str(e))

digit = FeatureRecognizer("DIGIT", DIGIT_LABELS, DIGIT_DIR,
    150, 20, 340, 200, (0, 0, 255), RGB888P_SIZE, ds)
digit.config_preprocess()
if not digit.load():
    digit.train(pl)

digit_filt = TemporalFilter(window=10, lock_in=4, lock_out=8)
track_filt = TemporalFilter(window=12, lock_in=5, lock_out=10)

print("\n[RUN] Line(CV every frame) + Digit(KPU every %d frames)" %
      KPU_INTERVAL)
if not uart_ok:
    print("[RUN] WARNING: UART not working - MSPM0 gets nothing")

frame = 0
d_label, t_label = None, None
angle, dev = 0, 0

while True:
    os.exitpoint()
    img_np = pl.get_frame()
    frame += 1

    # 快通道：每帧巡线 + 轨道类型
    angle, dev, cv_track, line_ok = detect_line(img_np)
    t_label, is_new = track_filt.update(cv_track)
    if t_label and is_new and uart:
        print("[TRACK] %s" % t_label)
        uart_send_track(uart, t_label)

    # 慢通道：KPU数字识别
    if frame % KPU_INTERVAL == 0:
        vec = digit.run(img_np)
        raw, score = digit.predict(vec)
        d_label, is_new = digit_filt.update(raw)
        if d_label and is_new:
            print("[DIGIT] %s" % d_label)
            if uart:
                uart_send_digit(uart, d_label)

    # UART巡线偏差 (15Hz)
    if frame % 2 == 0 and uart:
        uart_send_deviation(uart, dev)
        uart_send_status(uart, 0 if line_ok else 2)

    # 显示
    pl.osd_img.clear()
    digit.draw_box(pl, "D:%s" % (d_label or "?"))
    pl.osd_img.draw_string_advanced(10, 10, 20,
        "T:%s" % (t_label or "?"), color=(0, 255, 0))
    y_text = ds[1] - 24
    color = (0, 255, 0) if line_ok else (255, 0, 0)
    pl.osd_img.draw_string_advanced(10, y_text, 16,
        "a:%+d d:%+d %s" % (angle, dev, "OK" if line_ok else "LOST"),
        color=color)
    if not uart_ok:
        pl.osd_img.draw_string_advanced(10, 30, 16,
            "UART OFF!", color=(255, 0, 0))

    pl.show_image()

    if frame % 30 == 0:
        gc.collect()
