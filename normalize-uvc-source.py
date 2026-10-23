from pathlib import Path

base = Path('/boot/home/x270-camera-mic/haiku-uvc-webcam')
for relative in ('CamDevice.cpp', 'Producer.cpp', 'addons/uvc/UVCCamDevice.cpp'):
    path = base / relative
    content = path.read_bytes()
    path.write_bytes(content.replace(b'\r\n', b'\n'))

path = Path('/boot/home/haiku/src/apps/soundrecorder/RecorderWindow.cpp')
path.write_bytes(path.read_bytes().replace(b'\r\n', b'\n'))
