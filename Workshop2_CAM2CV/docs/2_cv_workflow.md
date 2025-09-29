# Python CV Workflow

Follow this playbook to prepare the Python environment, run the demo scripts, and integrate their outputs with the rest of the workshop.

## 1. Prepare the Environment

```bash
cd Workshop2_CAM2CV/cv-modules
python -m venv .venv
.venv\Scripts\Activate.ps1  # Windows PowerShell
# source .venv/bin/activate    # macOS/Linux
pip install --upgrade pip
pip install -r requirements.txt
```

Dependencies are lightweight (OpenCV, MediaPipe, NumPy, Requests) so the first install finishes quickly.

SoftAP portal is at `http://192.168.4.1/` by default; the MJPEG stream is at `http://192.168.4.1:81/stream`. If you switched the firmware to station mode, use the LAN IP printed on the serial monitor (append `:81` for the stream).

## 2. Confirm Connectivity

Grab a single JPEG to verify the board is reachable:

```bash
python - <<'PY'
import requests
url = "http://192.168.4.1/capture"  # replace with your device IP
resp = requests.get(url, timeout=5)
resp.raise_for_status()
print(f"Snapshot bytes: {len(resp.content)}")
with open("test_capture.jpg", "wb") as fh:
    fh.write(resp.content)
print("Saved test_capture.jpg")
PY
```

A non-zero byte count confirms the stream is alive.

## 3. Run the Demo Modules

| Demo | Command | Notes |
|------|---------|-------|
| Gesture recognition | `python gesture_detection.py --source http://<device-ip>:81/stream` | Backends: rules (MediaPipe Hands heuristics) or tasks (MediaPipe Tasks GestureRecognizer via `--gesture-backend tasks`). |
| Object detection | `python object_detection.py --source http://<device-ip>:81/stream --display` | EfficientDet Lite0 (TFLite); downloads ~4.3MB model on first run. |
| Image classification | `python image_classification.py --source http://<device-ip>:81/stream --display --top-k 5` | MobileNetV3 classifier with ImageNet labels. |

Note (Windows): the OpenCV preview window is disabled automatically to avoid a known GUI freeze. Use the browser preview in `webcam-starter/web/`. On macOS/Linux, `--display` works as usual.

All scripts accept `--source http://<device-ip>:81/stream` and `--save output.mp4` to archive annotated footage.


### WebSocket output to p5.js

`gesture_detection.py` can broadcast JSON over WebSocket (default `ws://<host>:8765/fireworks`). The front end consumes this in the Bubbles visual.

- Options: `--ws-host 0.0.0.0 --ws-port 8765 --ws-path /fireworks`
- In the front end (`webcam-starter/web/index.html`) Bubbles is loaded by default; you can override the WS via URL: `?ws=ws://localhost:8765/fireworks`

## 4. Extend the Demos

- Gestures: add custom gesture rules in `classify_gesture()` (rules backend) or switch to the Tasks backend (`--gesture-backend tasks`) for robust, pre-trained categories.
- For `object_detection.py` / `image_classification.py`, tweak `--max-results` or `--score` thresholds to observe model behaviour.
- Expose detections via a tiny Flask API and consume them from p5.js in the browser.

## 5. Troubleshooting

| Issue | Fix |
|-------|-----|
| `mediapipe` import fails | Ensure Python 3.10/3.11, then re-run `pip install -r requirements.txt`. |
| Stream lag | Lower the camera resolution in firmware or increase `--score`/decrease `--max-results`. |
| No GUI window | On macOS, launch from Terminal; ensure the app has screen recording permission if needed. |
| Model download fails | Manually download the required TFLite model into `resources/models/`. |

Once the Python modules are running smoothly, move on to `docs/3_visualization_workflow.md` to link detections with the p5.js playground.