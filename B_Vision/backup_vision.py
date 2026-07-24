"""
智能送药小车 - 完整视觉程序 v2
=============================
两个独立识别器（ROI不重叠）：
  识别器A (上部ROI,蓝框) - 数字识别 1~8
  识别器B (下部ROI,绿框) - 轨道识别 straight/left/right
交替推理 + UART发送给MSPM0
"""
import os, gc, time
from media.display import *
from machine import UART
from libs.PipeLine import PipeLine
from libs.AIBase import AIBase
from libs.AI2D import Ai2d
from libs.Utils import *
import nncase_runtime as nn
import ulab.numpy as np

# ==================== 配置 ====================
DISPLAY_MODE = "lcd"
RGB888P_SIZE = [640, 360]
MODEL_PATH = "/sdcard/examples/kmodel/recognition.kmodel"
MODEL_INPUT = [224, 224]

# 数字识别器 (上部，不跟下面重叠)
DIGIT_LABELS = ["1", "2", "3", "4", "5", "6", "7", "8"]
DIGIT_FEATURE_DIR = "/sdcard/features_digit/"
DIGIT_CROP = (150, 20, 340, 200)    # x, y, w, h  (y:20~220)

# 轨道识别器 (下部，不跟上面重叠)
TRACK_LABELS = ["straight", "left", "right"]
TRACK_FEATURE_DIR = "/sdcard/features_track/"
TRACK_CROP = (140, 230, 360, 120)    # x, y, w, h  (y:230~350)

FEATURES_PER = 3
FRAMES_PER_FEATURE = 40
THRESHOLD = 0.55

# ==================== 识别器类 ====================
class FeatureRecognizer(AIBase):
    def __init__(self, name, labels, feature_dir,
                 crop_x, crop_y, crop_w, crop_h,
                 box_color, rgb888p_size, display_size):
        super().__init__(MODEL_PATH, MODEL_INPUT, rgb888p_size, 0)
        self.name = name
        self.labels = labels
        self.feature_dir = feature_dir
        self.crop_x, self.crop_y = crop_x, crop_y
        self.crop_w, self.crop_h = crop_w, crop_h
        self.box_color = box_color  # (R,G,B) 区分两个识别器
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.feature_db = {}
        self.trained = False

        self.ai2d = Ai2d(0)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT,
                                   np.uint8, np.uint8)

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
        best_label, best_score = None, 0.0
        for label, feats in self.feature_db.items():
            for fvec in feats:
                sim = np.sum(vec * fvec) / (
                    np.sqrt(np.sum(vec*vec)) * np.sqrt(np.sum(fvec*fvec)))
                if sim > best_score:
                    best_score = sim
                    best_label = label
        if best_score >= THRESHOLD:
            return best_label, best_score
        return None, best_score

    def save(self):
        try:
            os.mkdir(self.feature_dir)
        except:
            pass  # 目录已存在
        for label, feats in self.feature_db.items():
            for i, vec in enumerate(feats):
                with open(self.feature_dir + label + "_" + str(i) + '.bin', 'wb') as f:
                    f.write(vec.tobytes())

    def load(self):
        try:
            for fn in os.listdir(self.feature_dir):
                if fn.endswith('.bin'):
                    label = fn.rsplit('_', 1)[0]
                    with open(self.feature_dir + fn, 'rb') as f:
                        vec = np.frombuffer(f.read(), dtype=np.float)
                    if label not in self.feature_db:
                        self.feature_db[label] = []
                    self.feature_db[label].append(vec)
            if self.feature_db:
                self.trained = True
                print("[%s] Loaded: %s" % (self.name,
                    str(sorted(self.feature_db.keys()))))
                return True
        except:
            pass
        return False

    def draw_box(self, pl, text):
        # 坐标从 AI 分辨率换算到显示分辨率
        ox = int(self.crop_x * self.display_size[0] / self.rgb888p_size[0])
        oy = int(self.crop_y * self.display_size[1] / self.rgb888p_size[1])
        ow = int(self.crop_w * self.display_size[0] / self.rgb888p_size[0])
        oh = int(self.crop_h * self.display_size[1] / self.rgb888p_size[1])
        pl.osd_img.draw_rectangle(ox, oy, ow, oh, color=self.box_color, thickness=3)
        if text:
            # 文字画在框内左上角，不会溢出
            pl.osd_img.draw_string_advanced(ox + 4, oy + 4, 20, text,
                                             color=self.box_color)

    def train(self, pl):
        print("\n[%s] === Training: %s ===" % (self.name, str(self.labels)))
        for label in self.labels:
            for fi in range(FEATURES_PER):
                tail_vecs = []  # 收集尾部帧用于平均
                for f in range(FRAMES_PER_FEATURE):
                    os.exitpoint()
                    img = pl.get_frame()
                    vec = self.run(img)
                    # 保留最后 5 帧的特征
                    tail_vecs.append(vec)
                    if len(tail_vecs) > 5:
                        tail_vecs.pop(0)

                    txt = "%s %s %d/%d f%d/%d" % (
                        self.name, label, fi+1, FEATURES_PER,
                        f+1, FRAMES_PER_FEATURE)
                    pl.osd_img.clear()
                    self.draw_box(pl, txt)
                    pl.show_image()

                # 用最后5帧的平均作为样本（更稳定）
                avg_vec = sum(tail_vecs) / len(tail_vecs)
                if label not in self.feature_db:
                    self.feature_db[label] = []
                self.feature_db[label].append(avg_vec)
        self.save()
        self.trained = True
        print("[%s] Training done!" % self.name)


# ==================== UART ====================
TRACK_VAL = {"straight": 0, "left": -80, "right": 80}

def uart_send(u, label):
    if label in TRACK_VAL:
        val = TRACK_VAL[label] & 0xFFFF
        dh, dl = (val >> 8) & 0xFF, val & 0xFF
        cs = (0x01 + dh + dl) & 0xFF
        u.write(bytes([0xAA, 0x01, dh, dl, cs, 0x55]))
    else:
        d = int(label) & 0xFF
        cs = (0x02 + 0 + d) & 0xFF
        u.write(bytes([0xAA, 0x02, 0, d, cs, 0x55]))


# ==================== 主流程 ====================
try:
    uart = UART(1, baudrate=115200)
    print("[INIT] UART OK")
except:
    uart = None
    print("[INIT] No UART")

print("[INIT] PipeLine...")
pl = PipeLine(rgb888p_size=RGB888P_SIZE, display_mode=DISPLAY_MODE)
pl.create()
ds = pl.get_display_size()

# 数字识别器 — 蓝色框
digit = FeatureRecognizer(
    "DIGIT", DIGIT_LABELS, DIGIT_FEATURE_DIR,
    *DIGIT_CROP,
    box_color=(0, 0, 255),   # 蓝
    rgb888p_size=RGB888P_SIZE, display_size=ds)
digit.config_preprocess()

# 轨道识别器 — 绿色框
track = FeatureRecognizer(
    "TRACK", TRACK_LABELS, TRACK_FEATURE_DIR,
    *TRACK_CROP,
    box_color=(0, 255, 0),   # 绿
    rgb888p_size=RGB888P_SIZE, display_size=ds)
track.config_preprocess()

# 加载或训练
if not digit.load():
    digit.train(pl)
if not track.load():
    track.train(pl)

# ==================== 时序滤波器 ====================
class TemporalFilter:
    def __init__(self, window=8, lock_in=4, lock_out=6):
        self.history = []
        self.window = window
        self.lock_in = lock_in
        self.lock_out = lock_out
        self.locked_label = None
        self.unlock_count = 0

    def update(self, label, score):
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


# ==================== 推理 ====================
print("\n[RUN] Blue=DIGIT  Green=TRACK  (filtered, no flicker)")

digit_filter = TemporalFilter(window=10, lock_in=4, lock_out=8)
track_filter = TemporalFilter(window=12, lock_in=5, lock_out=10)

# 双框常驻缓存
d_label, d_score = None, 0.0
t_label, t_score = None, 0.0
d_raw = None
t_raw = None
frame = 0

while True:
    os.exitpoint()
    img = pl.get_frame()
    frame += 1

    # 奇数帧更新数字，偶数帧更新轨道
    if frame & 1:
        vec = digit.run(img)
        raw_label, score = digit.predict(vec)
        d_raw = raw_label if (raw_label and score >= THRESHOLD) else None
        d_label, is_new = digit_filter.update(d_raw, score)
        if d_label and is_new:
            print("[DIGIT] LOCKED: %s" % d_label)
            if uart:
                uart_send(uart, d_label)
    else:
        vec = track.run(img)
        raw_label, score = track.predict(vec)
        t_raw = raw_label if (raw_label and score >= THRESHOLD) else None
        t_label, is_new = track_filter.update(t_raw, score)
        if t_label and is_new:
            print("[TRACK] LOCKED: %s" % t_label)
            if uart:
                uart_send(uart, t_label)

    # 双框常驻显示（不闪烁）
    pl.osd_img.clear()
    digit.draw_box(pl, "DIGIT: %s" % (d_label or "?"))
    track.draw_box(pl, "TRACK: %s" % (t_label or "?"))

    # 原始值小字（灰色）
    raw_txt = ""
    if d_raw: raw_txt += "D:" + d_raw + " "
    if t_raw: raw_txt += "T:" + t_raw
    if raw_txt:
        pl.osd_img.draw_string_advanced(10, 460, 14, raw_txt, color=(128,128,128))

    pl.show_image()
    gc.collect()
