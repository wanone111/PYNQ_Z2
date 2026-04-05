#!/usr/bin/env python3
"""USB camera to PL bridge skeleton for PYNQ-Z2.

This script captures frames from a USB camera on the PS side, prepares the
frame buffer, and pushes frames into the PL path through an overlay bridge.
The bridge supports a dry-run mode so the script can be executed on a normal
Linux host for validation.
"""

from __future__ import annotations

import argparse
import glob
import os
import time
from dataclasses import dataclass
from typing import Optional, Tuple

import cv2
import numpy as np


try:
    from pynq import Overlay, allocate  # type: ignore
except Exception:  # pragma: no cover - optional dependency on host
    Overlay = None
    allocate = None


@dataclass
class BridgeConfig:
    camera_index: int = 0
    width: int = 640
    height: int = 480
    fps: int = 30
    overlay_path: Optional[str] = None
    ip_name: str = "video_preprocess"
    dry_run: bool = True
    show: bool = False
    roi_x: int = 0
    roi_y: int = 0
    roi_w: int = 0
    roi_h: int = 0
    gray_en: int = 0
    threshold_en: int = 0
    threshold_val: int = 96
    probe_only: bool = False


class PlBridge:
    def __init__(self, config: BridgeConfig) -> None:
        self.config = config
        self.overlay = None
        self.ip = None
        self.dma = None
        self.tx_buffer = None
        self.rx_buffer = None

        if config.dry_run:
            return
        if Overlay is None:
            raise RuntimeError("pynq is not available; use --dry-run on a host machine")
        if not config.overlay_path:
            raise RuntimeError("overlay path is required when dry-run is disabled")

        self.overlay = Overlay(config.overlay_path)
        if hasattr(self.overlay, config.ip_name):
            self.ip = getattr(self.overlay, config.ip_name)
        else:
            raise RuntimeError(f"IP '{config.ip_name}' not found in overlay")

        # The exact DMA binding depends on the board integration.
        # Team members can map this attribute to the real AXI DMA / VDMA object.
        if hasattr(self.overlay, "axi_dma_0"):
            self.dma = getattr(self.overlay, "axi_dma_0")

        if self.dma is not None and allocate is not None:
            pixel_count = self.config.width * self.config.height
            self.tx_buffer = allocate(shape=(pixel_count,), dtype=np.uint32)
            self.rx_buffer = allocate(shape=(pixel_count,), dtype=np.uint32)

    def write_control(self) -> None:
        if self.ip is None:
            return
        self.ip.write(0x10, self.config.height)
        self.ip.write(0x18, self.config.width)
        self.ip.write(0x20, self.config.roi_x)
        self.ip.write(0x28, self.config.roi_y)
        self.ip.write(0x30, self.config.roi_w)
        self.ip.write(0x38, self.config.roi_h)
        self.ip.write(0x40, self.config.gray_en)
        self.ip.write(0x48, self.config.threshold_en)
        self.ip.write(0x50, self.config.threshold_val)
        self.ip.write(0x00, 0x01)

    def pack_rgb888(self, frame_bgr: np.ndarray) -> np.ndarray:
        rgb = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
        rgb = np.ascontiguousarray(rgb, dtype=np.uint8)
        packed = np.empty((self.config.height, self.config.width), dtype=np.uint32)
        packed[:] = (
            rgb[:, :, 0].astype(np.uint32) << 16
            | rgb[:, :, 1].astype(np.uint32) << 8
            | rgb[:, :, 2].astype(np.uint32)
        )
        return packed

    def send_frame(self, frame_bgr: np.ndarray) -> Tuple[np.ndarray, Optional[np.ndarray], float, float]:
        start_ns = time.time_ns()
        self.write_control()
        packed = self.pack_rgb888(frame_bgr)
        rx_packed = None
        tx_ms = 0.0
        rx_ms = 0.0

        if self.dma is not None:
            tx_view = self.tx_buffer if self.tx_buffer is not None else packed.reshape(-1).copy()
            rx_view = self.rx_buffer if self.rx_buffer is not None else np.empty_like(packed.reshape(-1))

            np.copyto(tx_view, packed.reshape(-1))
            dma_start_ns = time.time_ns()
            if hasattr(self.dma, "recvchannel"):
                self.dma.recvchannel.transfer(rx_view)
            self.dma.sendchannel.transfer(tx_view)
            self.dma.sendchannel.wait()
            if hasattr(self.dma, "recvchannel"):
                self.dma.recvchannel.wait()
                rx_packed = np.array(rx_view, copy=True)
            else:
                rx_packed = np.array(tx_view, copy=True)
            dma_end_ns = time.time_ns()
            tx_ms = (dma_end_ns - dma_start_ns) / 1_000_000.0
            rx_ms = tx_ms

        elapsed_ms = (time.time_ns() - start_ns) / 1_000_000.0
        return packed, rx_packed, elapsed_ms, tx_ms


def parse_args() -> BridgeConfig:
    parser = argparse.ArgumentParser(description="USB camera to PL bridge skeleton")
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--overlay", type=str, default=None)
    parser.add_argument("--ip-name", type=str, default="video_preprocess")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--show", action="store_true")
    parser.add_argument("--roi", type=str, default="0,0,0,0")
    parser.add_argument("--gray", type=int, default=0)
    parser.add_argument("--threshold-en", type=int, default=0)
    parser.add_argument("--threshold", type=int, default=96)
    parser.add_argument("--probe", action="store_true")
    args = parser.parse_args()

    roi_values = [float(v) for v in args.roi.split(",")]
    if len(roi_values) != 4:
        raise ValueError("--roi must be x,y,w,h")
    roi_x = int(roi_values[0] * args.width)
    roi_y = int(roi_values[1] * args.height)
    roi_w = int(roi_values[2] * args.width)
    roi_h = int(roi_values[3] * args.height)
    return BridgeConfig(
        camera_index=args.camera,
        width=args.width,
        height=args.height,
        fps=args.fps,
        overlay_path=args.overlay,
        ip_name=args.ip_name,
        dry_run=args.dry_run or args.overlay is None,
        show=args.show,
        roi_x=roi_x,
        roi_y=roi_y,
        roi_w=roi_w,
        roi_h=roi_h,
        gray_en=args.gray,
        threshold_en=args.threshold_en,
        threshold_val=args.threshold,
        probe_only=args.probe,
    )


def probe_camera(dev: str, tests: Tuple[Tuple[int, int, int], ...]) -> None:
    print("=== sysfs info ===")
    name_path = dev.replace("/dev/", "/sys/class/video4linux/") + "/name"
    if os.path.exists(name_path):
        with open(name_path, "r", encoding="utf-8") as handle:
            print("name:", handle.read().strip())

    print("\n=== supported formats (sysfs list) ===")
    video_name = os.path.basename(dev)
    for path in sorted(glob.glob(f"/sys/class/video4linux/{video_name}/*")):
        print(path)

    print("\n=== OpenCV test common modes ===")
    for width, height, fps in tests:
        cap = cv2.VideoCapture(dev, cv2.CAP_V4L2)
        if not cap.isOpened():
            cap = cv2.VideoCapture(dev)
        if not cap.isOpened():
            print(f"open failed: {dev}")
            break

        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        cap.set(cv2.CAP_PROP_FPS, fps)

        rw = cap.get(cv2.CAP_PROP_FRAME_WIDTH)
        rh = cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
        rfps = cap.get(cv2.CAP_PROP_FPS)
        ok, _frame = cap.read()
        print(f"req=({width}x{height}@{fps}) -> got=({rw}x{rh}@{rfps}) read_ok={ok}")

        cap.release()


def main() -> int:
    cfg = parse_args()
    camera_dev = f"/dev/video{cfg.camera_index}"

    if cfg.probe_only:
        probe_camera(
            camera_dev,
            (
                (1920, 1080, 30),
                (1280, 720, 30),
                (1280, 720, 15),
                (1280, 720, 10),
                (640, 480, 30),
                (640, 480, 60),
            ),
        )
        return 0

    cap = cv2.VideoCapture(cfg.camera_index, cv2.CAP_V4L2)
    if not cap.isOpened():
        print(f"[USB->PL] failed to open camera {cfg.camera_index}")
        return 1

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, cfg.width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, cfg.height)
    cap.set(cv2.CAP_PROP_FPS, cfg.fps)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    bridge = PlBridge(cfg)
    print(f"[USB->PL] camera={cfg.camera_index} {cfg.width}x{cfg.height}@{cfg.fps} dry_run={cfg.dry_run}")

    stats_window_start_ns = time.time_ns()
    stats_frames = 0
    stats_read_fail = 0
    stats_send_fail = 0
    stats_elapsed_sum_ms = 0.0
    stats_tx_sum_ms = 0.0
    stats_rx_sum_ms = 0.0

    try:
        while True:
            ok, frame = cap.read()
            if not ok or frame is None:
                stats_read_fail += 1
                continue

            if frame.shape[1] != cfg.width or frame.shape[0] != cfg.height:
                frame = cv2.resize(frame, (cfg.width, cfg.height), interpolation=cv2.INTER_LINEAR)

            try:
                packed, rx_packed, elapsed_ms, tx_ms = bridge.send_frame(frame)
            except Exception as exc:
                stats_send_fail += 1
                print(f"[USB->PL] send failed: {exc}")
                continue

            stats_frames += 1
            stats_elapsed_sum_ms += elapsed_ms
            stats_tx_sum_ms += tx_ms
            if rx_packed is not None:
                stats_rx_sum_ms += tx_ms

            print(f"[USB->PL] frame sent shape={packed.shape} bridge_ms={elapsed_ms:.2f}")

            now_ns = time.time_ns()
            window_s = (now_ns - stats_window_start_ns) / 1_000_000_000.0
            if window_s >= 1.0:
                fps = stats_frames / max(window_s, 1e-6)
                avg_bridge = stats_elapsed_sum_ms / max(stats_frames, 1)
                avg_tx = stats_tx_sum_ms / max(stats_frames, 1)
                avg_rx = stats_rx_sum_ms / max(stats_frames, 1) if stats_rx_sum_ms > 0 else 0.0
                dropped = stats_read_fail + stats_send_fail
                print(
                    f"[USB->PL] fps={fps:.2f} avg_bridge={avg_bridge:.2f}ms "
                    f"avg_tx={avg_tx:.2f}ms avg_rx={avg_rx:.2f}ms read_fail={stats_read_fail} "
                    f"send_fail={stats_send_fail} dropped_est={dropped}"
                )
                stats_window_start_ns = now_ns
                stats_frames = 0
                stats_read_fail = 0
                stats_send_fail = 0
                stats_elapsed_sum_ms = 0.0
                stats_tx_sum_ms = 0.0
                stats_rx_sum_ms = 0.0

            if cfg.show:
                preview = frame.copy()
                if cfg.roi_w > 0 and cfg.roi_h > 0:
                    cv2.rectangle(
                        preview,
                        (cfg.roi_x, cfg.roi_y),
                        (min(cfg.roi_x + cfg.roi_w, cfg.width - 1), min(cfg.roi_y + cfg.roi_h, cfg.height - 1)),
                        (0, 255, 0),
                        2,
                    )
                cv2.imshow("usb_camera_to_pl", preview)
                key = cv2.waitKey(1) & 0xFF
                if key in (27, ord("q"), ord("Q")):
                    break
    finally:
        cap.release()
        cv2.destroyAllWindows()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
