"""
钢珠检测 - 严格匹配官方demo，仅改display_mode
"""
from libs.PipeLine import PipeLine
from libs.YOLO import YOLO11
from libs.Utils import *
from media.sensor import *
import os, gc
import ulab.numpy as np
import image
import time

kmodel_path = "/data/marble_1.kmodel"
labels = {0: 'marble'}
model_input_size = [320, 320]

# DNK230D: lcd模式自动映射ST7701
display_mode = "lcd"
rgb888p_size = [640, 360]

pl = PipeLine(rgb888p_size=rgb888p_size, display_mode=display_mode)
pl.create()
display_size = pl.get_display_size()

confidence_threshold = 0.5
nms_threshold = 0.45
yolo = YOLO11(
    task_type="detect",
    mode="video",
    kmodel_path=kmodel_path,
    labels=labels,
    rgb888p_size=rgb888p_size,
    model_input_size=model_input_size,
    display_size=display_size,
    conf_thresh=confidence_threshold,
    nms_thresh=nms_threshold,
    max_boxes_num=10,
    debug_mode=0)
yolo.config_preprocess()

clock = time.clock()

while True:
    os.exitpoint()
    clock.tick()

    img = pl.get_frame()
    res = yolo.run(img)
    yolo.draw_result(res, pl.osd_img)
    print(res)
    pl.show_image()
    gc.collect()

    print("FPS:", clock.fps())

yolo.deinit()
pl.destroy()
