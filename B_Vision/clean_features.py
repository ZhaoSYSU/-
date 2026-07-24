"""
删除所有特征文件，重新训练前运行
"""
import os

dirs = ['/sdcard/features_digit/', '/sdcard/features_track/',
        '/sdcard/digit_features/', '/sdcard/features/']

for d in dirs:
    try:
        for f in os.listdir(d):
            os.remove(d + f)
        os.rmdir(d)
        print("Deleted: %s" % d)
    except:
        pass

print("Done. Run vision_full.py to re-train.")
