# Troubleshooting Companion

| Symptom | Root Cause | Fix |
|---------|------------|-----|
| Board not detected over USB | Missing driver or UF2 mode not triggered | Double-tap **BOOT** to enter UF2, install CP210x/CH9102 driver if needed. |
| Camera stream shows green/purple frames | Camera pins misconfigured or cable flipped | Confirm `firmware/xiao-s3-streaming/camera_pins.h` matches the Sense board mapping; reseat the ribbon with contacts facing the PCB. |
| MJPEG stream works but Python scripts freeze | Requests timeout | Check that the stream URL resolves from the laptop (ping the ESP32). Consider lowering `chunk_size` in `utils/stream_client.py`. |
| YOLO script crashes with `torch.cuda` error | GPU drivers missing | Set `export CUDA_VISIBLE_DEVICES=-1` (or Windows equivalent) to force CPU. |
| CLIP script out of memory | GPU VRAM too small | Add `--labels` with fewer prompts or force CPU execution. |
| p5.js page blocked from loading camera feed | Browser enforcing HTTPS-only content | Allow insecure content in site settings or serve Live Server over plain HTTP. |
| Fireworks visual never triggers | Motion threshold too high or lighting poor | Reduce threshold in `web/visualizations/fireworks.js` (`const threshold = 35`) or increase ambient light. |

Keep this table handy alongside the session agenda so you can respond quickly when learners hit roadblocks.
