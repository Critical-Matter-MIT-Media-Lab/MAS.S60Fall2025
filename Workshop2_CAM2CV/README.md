# MASS60 Computer Vision Workshop Toolkit

This repository packages everything required to run an entry-level computer vision workshop around the Seeed Studio XIAO ESP32S3 Sense. You can:

- Flash the XIAO with Arduino-ready firmware that exposes an MJPEG stream and snapshot endpoint.
- Consume the stream with Python demos covering gestures, object detection, and CLIP zero-shot recognition.
- Build browser-based visualisations with p5.js so participants see immediate feedback in their browser.
- Follow facilitator guides that explain the recommended agenda, checklists, and troubleshooting paths.

## Directory Guide

| Path | Purpose |
|------|---------|
| `firmware/xiao-s3-streaming/` | Arduino-style sketch code, camera pin map, and configuration header for the ESP32S3 stream server. |
| `cv-modules/` | Python scripts and shared utilities for computer vision demos, plus dependency requirements. |
| `webcam-starter/` | Live Server-friendly p5.js visual playground that layers effects on top of the MJPEG feed. |
| `resources/models/` | Optional cache for large pre-trained model weights used by the Python modules. |
| `docs/` | Facilitator documentation, workflow guides, checklist, agenda, and troubleshooting reference. |

## Quickstart Overview

1. **Firmware** - Follow `docs/1_firmware_setup.md` to upload the sketch. The default configuration brings up a SoftAP hotspot; add station credentials if you want the board on an existing router while keeping the hotspot available.
2. **Stream Test** - Connect to `XIAO-CV-Workshop` and open `http://192.168.4.1/`. The portal matches the official Seeed interface and lets you adjust camera settings. If the board also joined your router, you can use the LAN IP printed on the serial console.

> Stream endpoint: the MJPEG stream is served on port 81. Use `http://<board-ip>:81/stream` (SoftAP example: `http://192.168.4.1:81/stream`).

3. **Python Demos** - Set up a virtual environment via `docs/2_cv_workflow.md` and run the desired script (gestures, YOLO, CLIP). Each script prints the exact command line options it supports.
4. **Browser Visuals** - Launch `webcam-starter/web/index.html` with the VS Code Live Server extension and explore the provided visuals, or build your own by duplicating the templates.
5. **Facilitation** - Use the checklist, agenda, and troubleshooting docs in `docs/` to run the session smoothly.

### Quick Demo
For a 5-minute end-to-end demo (camera visual + gesture bubbles), see `docs/quick_demo.md`.


The remaining sections of this README point to deeper resources for each component.

## Firmware Highlights

- `config.h` contains all Wi-Fi and camera tuning options in one place. Switch between SoftAP and station mode, set frame size, JPEG quality, and LED behaviour without editing the sketch itself.
- `src/main.cpp` implements a minimal web portal so participants can see the video stream in any browser before they move to Python or p5.js.
- The firmware only depends on the core ESP32 Arduino libraries (`esp_camera`, `WiFi`, `WebServer`) so the Arduino IDE can compile it out of the box once the ESP32 board package is installed.
- Detailed flashing instructions and troubleshooting tips live in `firmware/xiao-s3-streaming/README.md` and `docs/1_firmware_setup.md`.

## Python Module Highlights

- `cv-modules/requirements.txt` specifies the tested versions for OpenCV, MediaPipe, ONNX Runtime, Ultralytics YOLO, PyTorch, OpenCLIP, and helper packages.
- Each script includes a module-level docstring that explains its pipeline, prerequisites, configurable flags, and example commands.
- Use `resources/models/` to pre-seed FER+, YOLOv8n, or other large weight files before the workshop if the venue network is restricted.

## Web Visualisation Highlights

- `webcam-starter/web/visual-engine.js` hosts a reusable p5.js engine that mirrors the MJPEG feed, manages visual selection, and exposes helper methods (`drawCamera`, `togglePause`, `saveFrame`).
- Two visuals are included out of the box: `template-minimal.js` (minimal template) and `bubbles.js` (gesture‑colored transparent bubbles). The Bubbles visual listens on `ws://<host>:8765/fireworks` by default; override via `?ws=ws://localhost:8765/fireworks` in the page URL.
- Full instructions for Live Server and suggested teaching flow are in `webcam-starter/README.md` and `docs/3_visualization_workflow.md`.

## Facilitator Resources

- `docs/agenda.md` proposes a two-hour schedule that walks from hardware setup to integrated demos.
- `docs/checklist.md` summarizes the preparation steps one week out, on the day of the session, during delivery, and after the workshop.
- `docs/troubleshooting.md` captures the most common hardware, network, and software issues with quick remedies.

## Support and Customisation

The repository is intentionally modular: you can replace any of the CV scripts, add new p5.js visuals, or adjust the firmware stream settings without breaking the other components. If you adapt it for different hardware or CV tasks, update the corresponding documentation so future facilitators stay aligned.