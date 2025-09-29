"""Object detection using MediaPipe Tasks (EfficientDet Lite0).

This script downloads the lightweight TFLite object detector the first time it
runs, then performs real-time detection on frames from the ESP32 stream. It's
much smaller than YOLO and runs well on CPU laptops.
"""

import argparse
import math
from pathlib import Path
from typing import List

import cv2
import mediapipe as mp
import numpy as np
import requests

from utils.overlays import draw_bbox, draw_hud
from utils.stream_client import FrameRateTracker, MJPEGStream

MODEL_URL = (
    "https://storage.googleapis.com/mediapipe-assets/efficientdet_lite0_fp16.tflite"
)
MODEL_PATH = Path(__file__).resolve().parent.parent / "resources" / "models" / "efficientdet_lite0_fp16.tflite"


def ensure_model() -> Path:
    MODEL_PATH.parent.mkdir(parents=True, exist_ok=True)
    if MODEL_PATH.exists() and MODEL_PATH.stat().st_size > 1024 * 1024:  # >1MB
        return MODEL_PATH

    print(f"[download] Fetching MediaPipe object detector from {MODEL_URL}")
    response = requests.get(MODEL_URL, timeout=30)
    response.raise_for_status()
    MODEL_PATH.write_bytes(response.content)
    print(f"[download] Saved to {MODEL_PATH}")
    return MODEL_PATH


def main() -> None:
    parser = argparse.ArgumentParser(description="MediaPipe EfficientDet object detection demo")
    parser.add_argument("--source", default="http://192.168.4.1/stream", help="MJPEG stream URL")
    parser.add_argument("--display", action="store_true", help="Show annotated frames")
    parser.add_argument("--save", type=str, help="Optional output video path")
    parser.add_argument("--score", type=float, default=0.3, help="Minimum confidence to display detections")
    parser.add_argument("--max-results", type=int, default=5, help="Maximum number of labels per frame")
    args = parser.parse_args()

    model_path = ensure_model()

    BaseOptions = mp.tasks.BaseOptions
    ObjectDetectorOptions = mp.tasks.vision.ObjectDetectorOptions
    ObjectDetector = mp.tasks.vision.ObjectDetector
    VisionRunningMode = mp.tasks.vision.RunningMode

    options = ObjectDetectorOptions(
        base_options=BaseOptions(model_asset_path=str(model_path)),
        score_threshold=args.score,
        max_results=args.max_results,
        running_mode=VisionRunningMode.IMAGE,
    )

    fps_tracker = FrameRateTracker()
    writer = None
    window_name = "MediaPipe Object Detection"

    with ObjectDetector.create_from_options(options) as detector:
        with MJPEGStream(args.source) as stream:
            for frame in stream.frames():
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                detection_result = detector.detect(mp_image)

                detections: List[mp.tasks.vision.Detection] = detection_result.detections or []
                for detection in detections:
                    category = detection.categories[0]
                    if category.score < args.score:
                        continue
                    bbox = detection.bounding_box
                    x1 = max(int(bbox.origin_x), 0)
                    y1 = max(int(bbox.origin_y), 0)
                    x2 = min(int(bbox.origin_x + bbox.width), frame.shape[1] - 1)
                    y2 = min(int(bbox.origin_y + bbox.height), frame.shape[0] - 1)
                    label = f"{category.category_name} {category.score:.2f}"
                    draw_bbox(frame, (x1, y1, x2, y2), label, color=(0, 200, 0))

                fps = fps_tracker.update()
                draw_hud(frame, [f"Source: {args.source}", f"FPS: {fps:.1f}", f"Detections: {len(detections)}"])

                if args.save and writer is None:
                    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
                    height, width = frame.shape[:2]
                    writer = cv2.VideoWriter(args.save, fourcc, max(fps, 15) or 15, (width, height))
                if writer is not None:
                    writer.write(frame)

                if args.display:
                    cv2.imshow(window_name, frame)
                    if cv2.waitKey(1) & 0xFF == ord("q"):
                        break

    if writer is not None:
        writer.release()
    if args.display:
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
