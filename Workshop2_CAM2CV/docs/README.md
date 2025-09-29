# MASS60 Workshop Docs

This directory gathers facilitator resources for the MASS60 computer vision workshop. Use these files alongside the firmware, Python modules, and web assets to run a smooth session.

## Document Map

| File | Summary |
|------|---------|
| `1_firmware_setup.md` | Step-by-step Arduino IDE workflow for the XIAO ESP32S3, plus optional PlatformIO notes. |
| `2_cv_workflow.md` | Instructions for bootstrapping the Python environment and running each demo module. |
| `3_visualization_workflow.md` | Guide for launching the p5.js Live Server playground and extending visuals. |
| `agenda.md` | Two-hour suggested timeline with milestones and speaking points. |
| `checklist.md` | Preparation checklist covering one week before, day-of, during, and after the workshop. |
| `troubleshooting.md` | Quick reference for common hardware, network, and software issues. |

## Suggested Flow

1. **Flash firmware** - Follow `1_firmware_setup.md` to upload the sketch. SoftAP is enabled by default, and you can optionally add station credentials so the board joins the venue Wi-Fi while keeping the hotspot alive.
2. **Verify stream** - Connect to `XIAO-CV-Workshop` or, if station mode succeeded, stay on the venue Wi-Fi and open the LAN IP printed on the serial monitor (SoftAP remains available at `http://192.168.4.1/`).
3. **Run Python demos** - Use `2_cv_workflow.md` to set up the virtual environment and explore the scripts in `cv-modules/`.
4. **Launch browser visuals** - Serve `webcam-starter/web/index.html` with Live Server per `3_visualization_workflow.md` and show the included overlays.
5. **Teach and iterate** - Use the agenda, checklist, and troubleshooting sheets during delivery.

Keep documentation in English for consistency with the code and UI assets.