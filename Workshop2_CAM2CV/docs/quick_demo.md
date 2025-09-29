# Quick Demo: Visualize the Camera and Drive Gesture Bubbles

This one‑pager walks you through two back‑to‑back demos you can run in class.

- A. Visualize the camera feed directly (no Python)
- B. Drive the Gesture Bubbles visual using Python CV (gestures)

## Prerequisites
- XIAO ESP32S3 Sense flashed with the streaming firmware
- SoftAP: `XIAO-CV-Workshop` (portal on 80), stream on port 81
- Or station mode: use the LAN IP printed in the serial console; the stream is `http://<ip>:81/stream`

---

## A) Visualize the camera (browser only)
1) Verify the stream in a browser tab:
   - SoftAP: `http://192.168.4.1:81/stream`
   - Station: `http://<board-ip>:81/stream`
2) Open the visual playground with VS Code Live Server:
   - Open `Workshop2_CAM2CV/webcam-starter/web/index.html`
   - Right‑click → Open with Live Server
3) In the page, set Camera stream URL to `http://<board-ip>:81/stream` and click Apply.
   - Use the Minimal HUD visual for a clean view.

---

## B) Gesture Bubbles visual driven by Python CV
1) Create and activate a virtual environment (from the repo root):
   - Windows PowerShell:
     ```powershell
     python -m venv .venv
     .venv\Scripts\Activate.ps1
     pip install -r Workshop2_CAM2CV/cv-modules/requirements.txt
     ```
   - macOS/Linux:
     ```bash
     python3 -m venv .venv
     source .venv/bin/activate
     pip install -r Workshop2_CAM2CV/cv-modules/requirements.txt
     ```
2) Run the gesture CV bridge:
   ```bash
   python Workshop2_CAM2CV/cv-modules/gesture_detection.py --webcam --gesture-backend tasks --gesture-model Workshop2_CAM2CV/resources/models/gesture_recognizer.task
   # or with device stream
   # python Workshop2_CAM2CV/cv-modules/gesture_detection.py --source http://<board-ip>:81/stream --gesture-backend tasks --gesture-model Workshop2_CAM2CV/resources/models/gesture_recognizer.task
   ```
   - Windows note: `--display` is disabled automatically to avoid an OpenCV GUI freeze; use the browser as your preview.
3) In the Live Server page, use the "Gesture Bubbles" visual (default).
   - The page auto‑connects to `ws://<your-computer-ip>:8765/fireworks`.
   - If Live Server runs on another machine, override via URL:
     - `index.html?ws=ws://<python-host-ip>:8765/fireworks`
4) Perform gestures; the bubbles change color in real time (idle does not change color; color switches trigger a halo).

---

## Troubleshooting quick list
- Blank canvas: confirm `http://<ip>:81/stream` works in a browser; allow mixed content if needed.
- Bubbles not reacting: ensure the Python script says `WebSocket server running on 0.0.0.0:8765` and the page is connected to that host.
- Windows freeze: run without `--display` (already default on Windows).
- Slow stream: lower frame size in firmware or adjust thresholds (`--score`, `--max-results`).

