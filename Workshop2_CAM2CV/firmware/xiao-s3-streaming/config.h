#pragma once
// config.h
// Defines network and camera streaming settings for the MASS60 workshop firmware.
// Adjust these values before compiling with the Arduino IDE so the ESP32S3 can join
// your classroom Wi-Fi or host its own hotspot, and so the MJPEG stream matches
// If station_ssid / station_password are customised (not "CHANGE_ME"), the firmware
// will attempt to join that network even when SoftAP mode is enabled.
// the resolution and quality you need for demos.

#include "esp_camera.h"

namespace workshop {

struct NetworkSettings {
    bool use_soft_ap;
    const char *soft_ap_ssid;
    const char *soft_ap_password;
    const char *station_ssid;
    const char *station_password;
    bool wait_for_station;
    uint8_t soft_ap_channel;
    bool soft_ap_hidden;
    uint8_t soft_ap_max_clients;
};

struct StreamSettings {
    framesize_t frame_size;
    pixformat_t pixel_format;
    int jpeg_quality;
    int frame_buffer_count;
    bool vertical_flip;
    bool horizontal_mirror;
    bool auto_white_balance;
    bool auto_gain_control;
    bool auto_exposure;
    int stream_delay_ms;
};

constexpr NetworkSettings kNetwork{
    /* use_soft_ap         */ true,
    /* soft_ap_ssid        */ "XIAO-CV-Workshop",
    /* soft_ap_password    */ "workshop123",
    /* station_ssid        */ "Olem",
    /* station_password    */ "123121qq",
    /* wait_for_station    */ false,
    /* soft_ap_channel     */ 6,
    /* soft_ap_hidden      */ false,
    /* soft_ap_max_clients */ 8
};

constexpr StreamSettings kStream{
    /* frame_size         */ FRAMESIZE_QVGA,   // 320x240 for stable streaming
    /* pixel_format       */ PIXFORMAT_JPEG,
    /* jpeg_quality       */ 12,               // 10-20 (lower is better quality)
    /* frame_buffer_count */ 2,
    /* vertical_flip      */ true,
    /* horizontal_mirror  */ true,
    /* auto_white_balance */ true,
    /* auto_gain_control  */ true,
    /* auto_exposure      */ true,
    /* stream_delay_ms    */ 33                // ~30 FPS target
};

constexpr uint16_t kWebServerPort = 80;
constexpr bool kEnableStatusLed = true;
constexpr uint8_t kStatusLedPin = 21;  // Onboard LED for the XIAO ESP32S3 Sense carrier

}  // namespace workshop
