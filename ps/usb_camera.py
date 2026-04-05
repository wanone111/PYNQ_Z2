import os
import glob

dev = "/dev/video0"

print("=== sysfs info ===")
name_path = "/sys/class/video4linux/video0/name"
if os.path.exists(name_path):
    with open(name_path, "r") as f:
        print("name:", f.read().strip())

print("\n=== supported formats (from v4l2-ctl substitute not complete) ===")
for p in glob.glob("/sys/class/video4linux/video0/*"):
    print(p)

print("\n=== OpenCV test common modes ===")
import cv2

tests = [
    (1920, 1080, 30),
    (1280, 720, 30),
    (1280, 720, 15),
    (1280, 720, 10),
    (640, 480, 30),
    (640, 480, 60),
]

for w, h, fps in tests:
    cap = cv2.VideoCapture(dev, cv2.CAP_V4L2)
    if not cap.isOpened():
        cap = cv2.VideoCapture(dev)
    if not cap.isOpened():
        print(f"open failed: {dev}")
        break

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, w)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, h)
    cap.set(cv2.CAP_PROP_FPS, fps)

    rw = cap.get(cv2.CAP_PROP_FRAME_WIDTH)
    rh = cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
    rfps = cap.get(cv2.CAP_PROP_FPS)

    ok, frame = cap.read()
    print(f"req=({w}x{h}@{fps}) -> got=({rw}x{rh}@{rfps}) read_ok={ok}")

    cap.release()