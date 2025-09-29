# Browser Visualisation Workflow

This guide explains how to run the p5.js playground in `webcam-starter/web/`, connect it to the ESP32 MJPEG stream, and extend it with custom overlays.

## 1. Serve the Files

### Option A - VS Code Live Server (recommended)
1. Install the Live Server extension.
2. Open `webcam-starter/web/index.html` in VS Code.
3. Right-click inside the editor and choose **Open with Live Server**. The page opens at `http://127.0.0.1:5500/` (the port may vary).

### Option B - Python HTTP server

If you cannot install extensions, run a lightweight server from the command line:

```bash
cd webcam-starter/web
python -m http.server 5500
```

Browse to `http://127.0.0.1:5500/` afterwards.

## 2. Connect to the ESP32 Stream

1. Type the camera URL (for example `http://192.168.4.1:81/stream`) into the **Camera stream URL** field.
2. Click **Apply**. The status indicator changes to Streaming... once frames arrive.
3. If the browser blocks mixed content (HTTPS page pulling HTTP video), allow insecure content in the site settings or run Live Server over plain HTTP only.

> Note: The ESP32 runs the web portal on port 80 and the MJPEG stream on port 81. Always use `http://<ip>:81/stream` (SoftAP: `http://192.168.4.1:81/stream` or the printed LAN IP with `:81`).


## 3. Visual Engine Basics

The core logic lives in `visual-engine.js` and is shared by all visuals. Key helpers:

- `engine.drawCamera(p, { mirror: true })` - draws the MJPEG frame inside the p5.js canvas.
- `engine.updateStream(url)` - swap to a different camera without reloading the page.
- `engine.togglePause()` - pause/resume the draw loop (also mapped to the spacebar).
- `engine.saveFrame()` - save a PNG snapshot (also mapped to the `S` key).

Keyboard shortcuts displayed in the UI:
- Number keys `1`-`9` switch between registered visuals.
- Spacebar toggles pause.
- `S` saves a PNG.

## 4. Included Visuals

| Visual | File | Description |
|--------|------|-------------|
| Minimal Template | `visualizations/template-minimal.js` | Mirrors the camera feed, displays stream status and FPS, and draws a simple frame outline. |
| Gesture Bubbles | `visualizations/bubbles.js` | Transparent bubbles with continuous drift; each gesture maps to a distinct color. Listens to the CV WebSocket bridge. |

Each visual registers itself by pushing an object into `window.VISUALS`. The object can define `init`, `draw`, and `dispose` hooks that receive the shared engine and p5.js instance.

## 4.5 Connect the CV WebSocket bridge

The Bubbles visual listens on a WebSocket for JSON payloads from the Python CV gesture bridge.

- Default endpoint: `ws://<host>:8765/fireworks` (auto-selected based on the page protocol)
- You can override via URL param: `index.html?ws=ws://localhost:8765/fireworks`
- Start the gesture bridge to broadcast:
  - `python Workshop2_CAM2CV/cv-modules/gesture_detection.py --webcam --gesture-backend tasks --gesture-model Workshop2_CAM2CV/resources/models/gesture_recognizer.task`
- Optional flags on the CV side: `--ws-host 0.0.0.0 --ws-port 8765 --ws-path /fireworks`

Windows note: the Python CV modules run headless by default (OpenCV preview is disabled) to avoid a known GUI freeze on Windows. Use the browser preview instead; `--display` is ignored on Windows and still works on macOS/Linux.

Payload shape (example):

```json
{
  "type": "gesture",
  "gesture": "open_palm",
  "confidence": 0.92
}
```

## 5. Building New Visuals

1. Duplicate `visualizations/template-minimal.js` and rename it (e.g., `my-visual.js`).
2. Update the `id`, `name`, and logic inside the `draw` function.
3. Add a `<script>` tag for your new file in `index.html` after `visual-engine.js`.
4. Reload the page or click **Apply** to refresh the stream; your new visual appears in the button list and is accessible via the next available number key.

Ideas:
- Render object detection boxes by polling a local Flask server that broadcasts YOLO results.
- Use audio synthesis in p5.js (`p5.sound`) to sonify detected gestures.
- Add timeline scrubbers or overlays that react to CLIP confidence scores.

## 6. Offline Considerations

- Download `p5.min.js` ahead of time, place it in `webcam-starter/web/lib/`, and change the `<script>` tag in `index.html` to point to the local file.
- If you use custom fonts or images, host them locally as well to avoid network surprises.

## 7. Troubleshooting

| Symptom | Fix |
|---------|-----|
| Canvas stays blank | Confirm the ESP32 stream works in a regular browser tab first, then check for mixed-content blocks. |
| FPS dips below 20 | Lower the ESP32 frame size in `config.h` or reduce the complexity of the visual (fewer particles, less per-frame processing). |
| Bubbles color never changes | Ensure the gesture bridge is running and payloads show `type:"gesture"`; idle gestures intentionally do not change color. |
| `CORS` errors when fetching local APIs | Serve the Python endpoint with `Access-Control-Allow-Origin: *` or use a simple proxy. |

With the browser visuals running, demonstrate how the ESP32 hardware, Python intelligence, and web front end work together end-to-end.