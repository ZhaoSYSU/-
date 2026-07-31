# ESP32-CAM 小球运动高速图传版

## 默认配置

- Wi-Fi 已写入工程；
- 摄像头端 V-Flip + H-Mirror，输出旋转180°后恢复正向；
- 默认 HQVGA 240×176、JPEG Quality 42；
- 双帧缓冲、`CAMERA_GRAB_LATEST`；
- Wi-Fi休眠关闭，发射功率提高；
- TCP_NODELAY开启；
- 逐帧FPS日志关闭；
- ESP32固定240 MHz。

## 网页

打开串口显示的根地址，例如：

`http://192.168.8.119/`

预设：

- 极致流畅：QQVGA 160×120，Quality 52；
- 小球运动：HQVGA 240×176，Quality 42；
- 普通流畅：QVGA 320×240，Quality 35；
- 清晰观察：CIF 400×296，Quality 26。

JPEG Quality 数字越大，压缩越强，通常越流畅。

## 使用建议

拍摄高速小球时优先选择“小球运动”；仍有少量卡顿时选择“极致流畅”。
只打开一个实时流页面，并使用稳定5 V、1 A以上电源。
