#pragma once
// camera_pins.h
// Provides the Seeed Studio XIAO ESP32S3 Sense pin mapping for the OV2640 sensor.
// Keep this file in sync with the Seeed documentation when using Arduino IDE sketches.

// Pin mapping for the Seeed Studio XIAO ESP32S3 Sense camera module (OV2640 sensor).
// Source: https://wiki.seeedstudio.com/XIAO_ESP32S3_Sense_Getting_Started/#pin-definition

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      10
#define SIOD_GPIO_NUM      40
#define SIOC_GPIO_NUM      39

#define Y9_GPIO_NUM        48
#define Y8_GPIO_NUM        11
#define Y7_GPIO_NUM        12
#define Y6_GPIO_NUM        14
#define Y5_GPIO_NUM        16
#define Y4_GPIO_NUM        18
#define Y3_GPIO_NUM        17
#define Y2_GPIO_NUM        15
#define VSYNC_GPIO_NUM     38
#define HREF_GPIO_NUM      47
#define PCLK_GPIO_NUM      13

#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_BASE_FREQ      (20000000)
