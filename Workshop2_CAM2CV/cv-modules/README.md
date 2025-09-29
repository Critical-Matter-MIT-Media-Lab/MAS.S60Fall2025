# Computer Vision Modules

Python demos that subscribe to the ESP32 MJPEG stream and run lightweight
MediaPipe pipelines. All modules share the same stream client utilities and run
entirely on CPU.

## Environment Setup

```bash
cd MASS60_CV_Workshop/cv-modules
python -m venv .venv
# Windows
.venv/Scripts/activate
# macOS / Linux
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

Dependencies: OpenCV, MediaPipe, NumPy, Requests (for model downloads).

## Modules

| Script | What it does | Key Flags |
|--------|--------------|-----------|
| `facial_expression_recognition.py` | MediaPipe Face Mesh + heuristics, classifies happy/surprised/angry/sad/neutral | `--display`, `--save`, `--min-score` |
| `gesture_detection.py` | MediaPipe Hands + rule-based gestures (open palm, fist, peace, thumbs, pinch) | `--display`, `--max-hands` |
| `object_detection.py` | MediaPipe EfficientDet Lite0 object detector | `--display`, `--save`, `--score`, `--max-results` |
| `image_classification.py` | MediaPipe MobileNet image classifier (ImageNet labels) | `--display`, `--top-k`, `--score` |


## WebSocket Bridge to p5.js

Both `facial_expression_recognition.py` and `gesture_detection.py` can broadcast live JSON to the front-end fireworks visual over WebSocket.

- Default endpoint: `ws://<host>:8765/fireworks`
- Override via flags: `--ws-host 0.0.0.0 --ws-port 8765 --ws-path /fireworks`
- Example run:
  - `python facial_expression_recognition.py --source http://192.168.4.1/stream`
  - `python gesture_detection.py --source http://192.168.4.1/stream`

Payload (example):
```json
{"type":"gesture","gesture":"open_palm","confidence":0.92,"hue":210,"launch_power":0.6,"spark_density":1.2,"twist":0.15,"center":{"x":0.52,"y":0.44},"style":"nebula"}
```

Open `webcam-starter/web/index.html` and select the Fireworks visual. If your WebSocket address differs, append `?ws=ws://localhost:8765/fireworks` to the page URL.

All scripts accept `--source http://<device-ip>/stream`. Default assumes the
ESP32 SoftAP (`http://192.168.4.1/stream`).

Each script can record annotated video via `--save output.mp4` and optionally
open a window with live overlays using `--display` (press `q` to quit).

## Shared Utilities

- `utils/stream_client.py` – MJPEG reader and simple FPS tracker.
- `utils/overlays.py` – Drawing helpers for HUD text and bounding boxes.

## Offline Assets

Model files download automatically to `resources/models/`. If the venue network
blocks downloads, pre-populate the following files:

| File | Used by |
|------|---------|
| `efficientdet_lite0_fp16.tflite` | `object_detection.py` |
| `mobilenet_v3_small.tflite`, `imagenet_labels.txt` | `image_classification.py` |

## Troubleshooting

- **No window appears** – when running from VS Code on macOS, launch from the Terminal app to ensure GUI permissions.
- **Stream lag** – lower the camera resolution (`config.h`) or increase `--score` / decrease `--max-results` for detections.
- **Model download blocked** – manually download the files listed above and place them in `resources/models/`.

Craft new modules by copying the existing pattern: read frames with `MJPEGStream`, run a MediaPipe task, draw overlays with `utils/overlays`, and expose `--display` / `--save` options for consistency.
