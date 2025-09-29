"""Image classification using MediaPipe MobileNetV3.

Reads frames from the ESP32 stream, runs a lightweight image classifier on the
center crop, and overlays the top-k labels. Ideal for quick demos without heavy
frameworks.
"""

import argparse
from pathlib import Path
from typing import List

import cv2
import mediapipe as mp
import numpy as np
import requests

from utils.overlays import draw_center_caption, draw_hud
from utils.stream_client import FrameRateTracker, MJPEGStream

MODEL_URL = "https://storage.googleapis.com/mediapipe-assets/mobilenet_v3_small.tflite"
LABELS_URL = "https://storage.googleapis.com/mediapipe-assets/imagenet_labels.txt"
MODEL_PATH = Path(__file__).resolve().parent.parent / "resources" / "models" / "mobilenet_v3_small.tflite"
LABELS_PATH = Path(__file__).resolve().parent.parent / "resources" / "models" / "imagenet_labels.txt"


def ensure_asset(url: str, path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.stat().st_size > 1024:
        return path
    print(f"[download] Fetching {url}")
    response = requests.get(url, timeout=30)
    response.raise_for_status()
    path.write_bytes(response.content)
    print(f"[download] Saved to {path}")
    return path


def load_labels(path: Path) -> List[str]:
    return [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def main() -> None:
    parser = argparse.ArgumentParser(description="MediaPipe MobileNet image classification demo")
    parser.add_argument("--source", default="http://192.168.4.1/stream", help="MJPEG stream URL")
    parser.add_argument("--display", action="store_true", help="Show annotated output window")
    parser.add_argument("--save", type=str, help="Optional MP4 output path")
    parser.add_argument("--top-k", type=int, default=3, help="Number of labels to show")
    parser.add_argument("--score", type=float, default=0.2, help="Minimum probability to display label")
    args = parser.parse_args()

    model = ensure_asset(MODEL_URL, MODEL_PATH)
    labels = load_labels(ensure_asset(LABELS_URL, LABELS_PATH))

    BaseOptions = mp.tasks.BaseOptions
    ImageClassifier = mp.tasks.vision.ImageClassifier
    ImageClassifierOptions = mp.tasks.vision.ImageClassifierOptions
    VisionRunningMode = mp.tasks.vision.RunningMode

    options = ImageClassifierOptions(
        base_options=BaseOptions(model_asset_path=str(model)),
        running_mode=VisionRunningMode.IMAGE,
        max_results=args.top_k,
        score_threshold=args.score,
    )

    fps_tracker = FrameRateTracker()
    writer = None
    window_name = "MediaPipe Image Classification"

    with ImageClassifier.create_from_options(options) as classifier:
        with MJPEGStream(args.source) as stream:
            for frame in stream.frames():
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                result = classifier.classify(mp_image)

                predictions = []
                for classification in result.classifications:
                    for category in classification.categories:
                        if category.score >= args.score:
                            predictions.append((category.category_name, category.score))

                predictions.sort(key=lambda item: item[1], reverse=True)
                predictions = predictions[: args.top_k]

                fps = fps_tracker.update()
                draw_hud(frame, [f"Source: {args.source}", f"FPS: {fps:.1f}"])

                if predictions:
                    caption = ", ".join(f"{label}: {score:.2f}" for label, score in predictions)
                    draw_center_caption(frame, caption, y_offset=50)

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
