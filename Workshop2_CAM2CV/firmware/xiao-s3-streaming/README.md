# XIAO ESP32S3 Streaming Firmware

This sketch turns the Seeed Studio XIAO ESP32S3 Sense (or XIAO ESP32S3 plus the Sense camera board) into a self-hosted MJPEG camera server. It exposes:

- `http://<device-ip>/` - control portal with live preview and camera parameter sliders
- `http://<device-ip>/stream` - a multipart MJPEG stream used by the Python demos and p5.js visuals
- `http://<device-ip>/capture` - a one-shot JPEG snapshot for quick debugging or dataset capture

The code lives in `src/main.cpp` and uses only the Arduino ESP32 core libraries (`esp_camera`, `WiFi`, `WebServer`). That keeps the workflow simple inside the Arduino IDE while still being compatible with PlatformIO if you prefer that toolchain.

## File Map

| File | Description |
|------|-------------|
| `xiao-s3-streaming.ino` | Minimal stub so the Arduino IDE can open the project without copying files. |
| `src/main.cpp` | Main Arduino sketch with camera init, Wi-Fi handling, status LED helpers, and HTTP routes. |
| `config.h` | Central place to configure Wi-Fi credentials, SoftAP defaults, frame size, JPEG quality, and stream behaviour. |
| `camera_pins.h` | Pin mapping for the OV2640 sensor on the Sense carrier board (copied from Seeed documentation). |

## Arduino IDE Setup (recommended for workshops)

1. Install Arduino IDE 2.x.
2. Open **Preferences** and add this URL to *Additional Boards Manager URLs*: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Open **Tools > Board > Boards Manager**, search for **esp32**, and install the package from Espressif Systems.
4. Select **Seeed XIAO ESP32S3** from **Tools > Board > ESP32 Arduino** and set **PSRAM** to **Enabled**.
5. Use **File > Open...** to load `firmware/xiao-s3-streaming/xiao-s3-streaming.ino`. The IDE will automatically show `config.h` and `camera_pins.h` beside the sketch.

## Configure the Stream Before Compiling

SoftAP mode is enabled by default so the board creates its own hotspot. If you also provide `station_ssid`/`station_password`, the firmware connects to that router while keeping the hotspot alive. Prefer only STA mode? Set `use_soft_ap = false` and fill in the credentials.

```cpp
constexpr NetworkSettings kNetwork{
    /* use_soft_ap         */ true,
    /* soft_ap_ssid        */ "XIAO-CV-Workshop",
    /* soft_ap_password    */ "workshop123",
    /* station_ssid        */ "CHANGE_ME",
    /* station_password    */ "CHANGE_ME",
    /* wait_for_station    */ false,
    /* soft_ap_channel     */ 6,
    /* soft_ap_hidden      */ false,
    /* soft_ap_max_clients */ 8
};
```

- **SoftAP workflow (default):** leave `use_soft_ap = true`. After flashing, computers and phones join the `XIAO-CV-Workshop` network (password `workshop123`) and visit `http://192.168.4.1/`. If you also configured station credentials, check the serial monitor for the additional LAN IP and use that when you need internet access through the venue router.
- **Join existing Wi-Fi only:** set `use_soft_ap = false`, replace `station_ssid` and `station_password`, and set `wait_for_station = true` if you want the board to reboot until it successfully connects.

The `kStream` structure lets you adjust frame size, JPEG quality, buffer count, image flip, and inter-frame delay. Start with `FRAMESIZE_QVGA` for the most stable 30 FPS stream. Increase to `FRAMESIZE_VGA` or higher only if the Wi-Fi link is strong.

## Flashing and Verifying (Arduino IDE)

1. Double-tap the **BOOT** button on the XIAO (only needed the first time) so it enters UF2 mode and appears as a USB storage device.
2. Click **Verify** in the Arduino IDE to compile the sketch; fix any typos in `config.h` if compilation fails.
3. Click **Upload** to flash. The IDE monitors the serial port and resets the board automatically.
4. Open **Tools > Serial Monitor** at 115200 baud. You should see:
   - `[camera]` messages confirming the sensor initialised
   - `[wifi] SoftAP "<ssid>" active` (or connection progress if using STA mode)
   - `[server] Ready on port 80`
5. Connect a laptop/phone to the printed network and visit `http://192.168.4.1/` (SoftAP). If station mode also connected, the serial console prints an additional LAN IP you can browse to from the same router.

## Troubleshooting

- `CAMERA init failed`: reseat the ribbon cable, reduce `kStream.frame_size`, ensure PSRAM is enabled, or power directly from your computer (avoid weak USB hubs).
- Stream is slow or corrupted: increase `kStream.jpeg_quality` (higher number = lower bandwidth) or lower the frame size.
- Cannot reach `/stream`: confirm your device is on the same network, disable VPNs, and temporarily allow the ESP32 IP in your firewall.
- Station mode keeps rebooting: wrong Wi-Fi password or unsupported 5 GHz network. Switch to SoftAP or move to a 2.4 GHz SSID.

With the stream working, you can jump into the Python and p5.js demos without changing the firmware again.