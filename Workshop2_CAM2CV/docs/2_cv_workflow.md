# Python CV Workflow

Follow this playbook to prepare the Python environment, run the demo scripts, and integrate their outputs with the rest of the workshop.

## 1. Prepare the Environment

```bash
cd MASS60_CV_Workshop/cv-modules
python -m venv .venv
.venv/Scripts/activate  # Windows
# source .venv/bin/activate  # macOS/Linux
pip install --upgrade pip
pip install -r requirements.txt
```

Dependencies are lightweight (OpenCV, MediaPipe, NumPy, Requests) so the first install finishes quickly even在受限網路。

SoftAP mode exposes the stream at `http://192.168.4.1/` by default. If you switched the firmware to station mode, substitute the IP printed on the serial monitor.

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
| Facial expression heuristics | `python facial_expression_recognition.py --display` | Uses MediaPipe Face Mesh + rules (happy/surprised/angry/sad/neutral). |
| Gesture recognition | `python gesture_detection.py --display` | MediaPipe Hands with simple gesture heuristics (open palm、fist、peace、pinch等). |
| Object detection | `python object_detection.py --display` | MediaPipe EfficientDet Lite0; downloads ~4.3MB TFLite model on first run. |
| Image classification | `python image_classification.py --display --top-k 5` | MobileNetV3 classifier with ImageNet labels。 |

All scripts accept `--source http://<device-ip>/stream` and `--save output.mp4` to archive annotated footage.


### WebSocket output to p5.js

`facial_expression_recognition.py` 與 `gesture_detection.py` 也可透過 WebSocket 推送 JSON（預設 `ws://<host>:8765/fireworks`）。

- 可用參數：`--ws-host 0.0.0.0 --ws-port 8765 --ws-path /fireworks`
- 在前端 `webcam-starter/web/index.html` 中選擇 Fireworks，或使用網址覆蓋：`?ws=ws://localhost:8765/fireworks`

## 4. Extend the Demos

- 修改 `facial_expression_recognition.py` 的規則以識別更多細微表情。
- 在 `gesture_detection.py` 的 `classify_gesture` 中加入自訂手勢，立刻可視化。
- 對 `object_detection.py` / `image_classification.py` 限制 `--max-results` 或改 `--score`，觀察模型反應。
- 建立小型 Flask API 轉出偵測結果，再由 p5.js 端抓取渲染。

## 5. Troubleshooting

| Issue | Fix |
|-------|-----|
| `mediapipe` import fails | 確認 Python 版本 3.10/3.11，重新執行 `pip install -r requirements.txt`。 |
| Stream lag | 降低攝像頭解析度或提高 `--score`、降低 `--max-results`。 |
| No GUI window | 在 macOS 從 Terminal 啟動，或確保應用具有螢幕錄製權限。 |
| 模型下載失敗 | 手動下載對應的 TFLite 模型放入 `resources/models/`。 |

Once the Python modules are running smoothly, move on to `docs/3_visualization_workflow.md` to link detections with the p5.js playground.