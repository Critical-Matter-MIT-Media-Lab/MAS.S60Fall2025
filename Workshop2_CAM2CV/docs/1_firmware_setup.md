# Firmware Preparation (Arduino IDE)

Use this guide to configure, compile, and upload the MASS60 streaming firmware to the Seeed Studio XIAO ESP32S3 Sense using the Arduino IDE. PlatformIO instructions remain available at the end for reference, but Arduino is the default path for workshops.

## 1. Hardware and Accounts

- Seeed Studio XIAO ESP32S3 Sense or XIAO ESP32S3 with the Sense camera board attached
- USB-C data cable
- Computer running Windows, macOS, or Linux with Arduino IDE 2.x installed

## 2. Prepare the Arduino IDE

1. Open **File > Preferences**.
2. Add the ESP32 package URL to *Additional Boards Manager URLs*: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Open **Tools > Board > Boards Manager**, search for **esp32**, and click **Install** for the Espressif Systems package.
4. Select **Seeed XIAO ESP32S3** from **Tools > Board > ESP32 Arduino** and set **PSRAM** to **Enabled**.
5. Use **File > Open...** to load `firmware/xiao-s3-streaming/xiao-s3-streaming.ino`. The IDE automatically shows `config.h` and `camera_pins.h` alongside the sketch.

## 3. Configure `config.h`

SoftAP mode is enabled by default so the board becomes its own hotspot (`XIAO-CV-Workshop`, password `workshop123`). Students join that network and browse to `http://192.168.4.1/` without needing a separate router. If you also fill in `station_ssid`/`station_password`, the firmware connects to that router while keeping the hotspot active, giving you both internet access and a fallback stream.

```cpp
constexpr NetworkSettings kNetwork{
    true,                 // use_soft_ap (default hotspot mode)
    "XIAO-CV-Workshop",   // SoftAP SSID
    "workshop123",        // SoftAP password (>= 8 characters)
    "CHANGE_ME",          // station SSID placeholder
    "CHANGE_ME",          // station password placeholder
    false,                 // wait_for_station (not used when SoftAP is active)
    6,                     // SoftAP Wi-Fi channel
    false,                 // keep SSID visible
    8                      // allow up to eight clients
};
```

If you prefer to use only the venue Wi-Fi, set `use_soft_ap = false`, fill in `station_ssid` and `station_password`, and consider setting `wait_for_station = true` so the board reboots until it connects.

Adjust `kStream` to change resolution, JPEG quality (higher number means higher compression), frame buffer count, and optional flips. `FRAMESIZE_QVGA` is the most stable starting point; only switch to `FRAMESIZE_VGA` or higher if the Wi-Fi link is strong.

## 4. Upload the Sketch

1. Double-tap the **BOOT** button on the XIAO to enter UF2 mode (RGB LED pulses green). This is only needed for the first upload.
2. Click **Verify** in the Arduino IDE to ensure the sketch compiles.
3. Click **Upload**. The IDE resets the board and flashes the firmware.
4. Open the **Serial Monitor** at 115200 baud. You should see:
   - `[camera]` lines confirming the OV2640 initialised
   - `[wifi] SoftAP "XIAO-CV-Workshop" active` (and, if station credentials are set, progress logs plus the LAN IP)
   - `[server] Ready on port 80`

## 5. Validate the Stream

1. Connect your laptop/phone to the `XIAO-CV-Workshop` network (password `workshop123`). If station mode also succeeded, you can keep your laptop on the venue Wi-Fi and browse to the printed LAN IP instead.
2. Visit `http://192.168.4.1/` to open the onboard portal. The control panel mirrors the official Seeed interface, allowing you to tweak resolution and other camera parameters on the fly. The page exposes the same controls as the official Seeed demo (resolution, quality, gain, white balance, mirror/flip, etc.) so you can tune the stream without modifying code.
3. Confirm the live stream (`/stream`) renders and that `/capture` returns a still JPEG.

## 6. Optional: PlatformIO Workflow

If you prefer PlatformIO, open the folder `firmware/xiao-s3-streaming/` in VS Code. The `platformio.ini` file already targets `seeed_xiao_esp32s3` with PSRAM enabled. Run:

```bash
pio run
pio run --target upload
pio device monitor -b 115200
```

## 7. Troubleshooting Quick Reference

| Symptom | Fix |
|---------|-----|
| `CAMERA init failed` | Reseat the ribbon cable, lower `kStream.frame_size`, ensure PSRAM is enabled, power from a stronger USB port. |
| Stream stuck on first frame | Increase `kStream.frame_buffer_count` to 2, or reduce resolution. |
| Cannot reach the board | Connect to `XIAO-CV-Workshop` (SoftAP) or use the LAN IP printed on the serial monitor when STA mode succeeds. |
| Station mode keeps rebooting | Wrong Wi-Fi password or 5 GHz network. Use 2.4 GHz or return to SoftAP mode. |
| Sketch fails to compile | Confirm you selected **Seeed XIAO ESP32S3** and have the ESP32 board package installed. |

Once the board is streaming, continue with `docs/2_cv_workflow.md` to set up the Python demos.