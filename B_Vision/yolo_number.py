"""
数字识别 0-9 - YOLO11 KPU推理
==============================
"""
from libs.PipeLine import PipeLine
from libs.YOLO import YOLO11
from libs.Utils import *
from media.sensor import *
import os, gc
import ulab.numpy as np
import image
import time

kmodel_path = "/data/number.kmodel"
labels = {0: '0', 1: '1', 2: '2', 3: '3', 4: '4',
          5: '5', 6: '6', 7: '7', 8: '8', 9: '9'}
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
    max_boxes_num=20,
    debug_mode=0)
yolo.config_preprocess()

clock = time.clock()

while True:
    os.exitpoint()
    clock.tick()

    img = pl.get_frame()
    try:
        res = yolo.run(img)
        yolo.draw_result(res, pl.osd_img)
        print(res)
    except Exception as e:
        print("[ERR] run/draw: %s" % str(e))
        import sys
        sys.print_exception(e)
        res = []

    pl.show_image()
    gc.collect()

    if res:
        for det in res:
            try:
                cls_id = int(det[5])
                print("  %s conf=%.2f" % (labels.get(cls_id, str(cls_id)), det[4]))
            except:
                print("  det format: %s" % str(det))
    else:
        print("  no detection")

yolo.deinit()
pl.destroy()
