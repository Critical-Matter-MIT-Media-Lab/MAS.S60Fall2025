// main.cpp
// MASS60 workshop firmware entry point for Seeed XIAO ESP32S3 Sense.
// Delegates HTTP UI and stream control to app_httpd.cpp (mirrors Seeed official GUI).
#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <cstring>

#include "camera_pins.h"
#include "config.h"

using workshop::kNetwork;
using workshop::kStream;

void startCameraServer();

namespace {

void setStatusLed(bool on) {
    if (!workshop::kEnableStatusLed) {
        return;
    }
    pinMode(workshop::kStatusLedPin, OUTPUT);
    digitalWrite(workshop::kStatusLedPin, on ? HIGH : LOW);
}

void blinkStatus(uint8_t times, uint16_t interval_ms = 150) {
    if (!workshop::kEnableStatusLed) {
        return;
    }
    for (uint8_t i = 0; i < times; ++i) {
        setStatusLed(true);
        delay(interval_ms);
        setStatusLed(false);
        delay(interval_ms);
    }
}

bool initCamera() {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL;
    config.ledc_timer = LEDC_TIMER;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = LEDC_BASE_FREQ;
    config.pixel_format = kStream.pixel_format;

    if (config.pixel_format == PIXFORMAT_JPEG) {
        config.frame_size = kStream.frame_size;
        config.jpeg_quality = kStream.jpeg_quality;
        config.fb_count = kStream.frame_buffer_count;
    } else {
        config.frame_size = FRAMESIZE_VGA;
        config.fb_count = 1;
    }

    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[camera] init failed: 0x%04x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, kStream.vertical_flip);
    s->set_hmirror(s, kStream.horizontal_mirror);
    s->set_whitebal(s, kStream.auto_white_balance);
    s->set_gain_ctrl(s, kStream.auto_gain_control);
    s->set_exposure_ctrl(s, kStream.auto_exposure);
    s->set_framesize(s, kStream.frame_size);

    return true;
}

void connectWiFi() {
    const bool wants_station = std::strcmp(kNetwork.station_ssid, "CHANGE_ME") != 0 &&
                               std::strlen(kNetwork.station_ssid) > 0;
    const bool start_soft_ap = kNetwork.use_soft_ap;

    if (start_soft_ap && wants_station) {
        WiFi.mode(WIFI_AP_STA);
    } else if (start_soft_ap) {
        WiFi.mode(WIFI_AP);
    } else {
        WiFi.mode(WIFI_STA);
    }

    if (start_soft_ap) {
        bool result = WiFi.softAP(kNetwork.soft_ap_ssid, kNetwork.soft_ap_password,
                                  kNetwork.soft_ap_channel, kNetwork.soft_ap_hidden,
                                  kNetwork.soft_ap_max_clients);
        if (!result) {
            Serial.println(F("[wifi] SoftAP start failed"));
        } else {
            Serial.printf("[wifi] SoftAP \"%s\" active\n", kNetwork.soft_ap_ssid);
            Serial.print(F("[wifi] AP IP address: "));
            Serial.println(WiFi.softAPIP());
        }
    }

    bool station_connected = false;
    if (wants_station) {
        WiFi.begin(kNetwork.station_ssid, kNetwork.station_password);
        Serial.printf("[wifi] Connecting to %s", kNetwork.station_ssid);

        const uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000) {
            Serial.print('.');
            blinkStatus(1, 125);
            delay(250);
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            station_connected = true;
            Serial.print(F("[wifi] Connected. IP address: "));
            Serial.println(WiFi.localIP());
            setStatusLed(true);
        } else {
            Serial.println(F("[wifi] Connection timeout"));
        }
    } else if (!start_soft_ap) {
        Serial.println(F("[wifi] Station credentials not set. Update config.h or enable SoftAP."));
    }

    if (!station_connected && kNetwork.wait_for_station && !start_soft_ap) {
        ESP.restart();
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();
    Serial.println(F("MASS60 XIAO ESP32S3 Camera Booting"));

    if (!initCamera()) {
        Serial.println(F("[camera] Halting due to init failure"));
        blinkStatus(10, 50);
        while (true) {
            delay(1000);
        }
    }

    connectWiFi();
    startCameraServer();

    Serial.println(F("[server] Camera portal ready. Open http://192.168.4.1/ (SoftAP) or the printed LAN IP."));
}

void loop() {
    delay(10000);
}