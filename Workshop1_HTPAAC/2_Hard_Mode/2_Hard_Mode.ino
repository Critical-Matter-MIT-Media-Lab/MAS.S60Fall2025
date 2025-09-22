/*
╔══════════════════════════════════════════════════════════════════════════╗
║              GSR SENSOR WITH WEB SERIAL COMMUNICATION                     ║
║                    Advanced Version with p5.js Integration                ║
╚══════════════════════════════════════════════════════════════════════════╝

 P5.JS SKETCH:
 ------------
 https://editor.p5js.org/YuxiangCheng/sketches/JNjPhiRBK

 ABOUT THIS CODE:
 ---------------
 This advanced version builds upon the basic GSR sensor by adding Web Serial
 communication for real-time data visualization in p5.js. We use our custom
 GSRVisualizer library to maintain clean, readable code while handling
 complex features.

 THE GSRVISUALIZER LIBRARY:
 -------------------------
 We created this custom library to modularize and simplify the code:
 • Separates complex algorithms from main logic
 • Makes the code more maintainable and readable
 • Allows easy reuse of functions between projects
 • Keeps advanced features organized and accessible

 Library Components:
 • GSRVisualizer.h - Header file with class definitions
 • GSRVisualizer.cpp - Implementation of all methods
 • Handles: signal processing, LED animations, web serial, and more

 KEY FEATURES:
 ------------
 • Real-time GSR data streaming to browser (JSON format)
 • Bidirectional communication with p5.js
 • Advanced LED animations with trail effects
 • Group-based color coding for workshops
 • Command processing from web interface

 HARDWARE:
 --------
 • GSR Sensor → GPIO 2 (ESP32) or A0 (Arduino)
 • LED Strip → GPIO 3 (ESP32) or Pin 6 (Arduino)
 • Serial → 115200 baud for Web Serial API
*/

#include <Adafruit_NeoPixel.h>
#include "GSRVisualizer.h"  // Our custom library for clean, modular code

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         HARDWARE CONFIGURATION
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

const int GSR_PIN = 2;     // Analog pin for GSR sensor
const int LED_PIN = 3;     // Digital pin for LED strip
const int NUM_LEDS = 20;   // Number of LEDs

/*████████████████████████████████████████████████████████████████████
██ CHALLENGE #1: SET YOUR GROUP NUMBER!                               ██
██ ➤ Change to your group number (1-5)                                ██
██ ➤ Each group gets a unique color:                                  ██
██   1=Red, 2=Green, 3=Blue, 4=Orange, 5=Purple                       ██
████████████████████████████████████████████████████████████████████*/
const int GROUP_NUMBER = 1; // Your group number (1-5)

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                          GLOBAL OBJECTS
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
GSRVisualizer* visualizer;

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         SENSOR VARIABLES
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

int gsrValue = 0;
int gsrMin = 1023;
int gsrMax = 0;
bool isCalibrated = false;

//──────── Filtering Variables ────────
int average = 0;
float emaValue = 0;
float lastEmaValue = 0;
float emaDerivative = 0;
float baseline = 0;

//──────── Web Serial Variables ────────
String inputBuffer = "";
unsigned long lastSendTime = 0;

/*████████████████████████████████████████████████████████████████████
██ CHALLENGE #2: ADJUST DATA SENDING FREQUENCY!                       ██
██ ➤ 20 = Send data 50 times/second (very responsive)                 ██
██ ➤ 50 = Send data 20 times/second (current, balanced)              ██
██ ➤ 100 = Send data 10 times/second (less network traffic)          ██
████████████████████████████████████████████████████████████████████*/
const int SEND_INTERVAL = 50;   // Send data every 50ms (20Hz)

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                            SETUP
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void setup() {
  // Initialize serial for web communication
  Serial.begin(115200);

  // Initialize LED strip
  strip.begin();
  strip.setBrightness(50);
  strip.show();

  // Create visualizer
  visualizer = new GSRVisualizer(strip, 10);
  visualizer->setGroupNumber(GROUP_NUMBER);

  /*████████████████████████████████████████████████████████████████████
  ██ CHALLENGE #3: MODIFY ANIMATION TRAIL LENGTH!                       ██
  ██ ➤ In GSRVisualizer constructor above, change the trail length:     ██
  ██ ➤ Default trail = 5 LEDs                                           ██
  ██ ➤ Try: 3 (short trail), 10 (long trail), 15 (very long)           ██
  ██ ➤ To change: Edit the trailLength variable in the library          ██
  ████████████████████████████████████████████████████████████████████*/

  // Send ready signal
  visualizer->sendStatus("READY");

  // Auto-calibrate
  performCalibration();
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                           MAIN LOOP
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void loop() {
  // 1. Handle incoming commands
  handleSerialInput();

  // 2. Read and process sensor
  gsrValue = analogRead(GSR_PIN);
  average = visualizer->calculateMovingAverage(gsrValue);

  // 3. Update EMA filtering
  lastEmaValue = emaValue;
  emaValue = visualizer->applyExponentialFilter(gsrValue, emaValue);
  emaDerivative = emaValue - lastEmaValue;

  // 4. Send data to p5.js
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    visualizer->sendDataToP5(emaValue);
    lastSendTime = millis();
  }

  // 5. Update LED visualization
  // Use the simple visualization by default (same as Hardware Starter)
  // Advanced modes can still be triggered via web serial commands
  if (visualizer->getLEDMode() == MODE_GSR_VISUALIZATION) {
    // Use the simple, speed-based visualization
    visualizer->updateLEDDisplay(emaValue, gsrMin, gsrMax);
  } else {
    // Use advanced modes if commanded from p5.js
    visualizer->updateLEDs(emaValue, gsrMin, gsrMax, emaDerivative);
  }

  /*████████████████████████████████████████████████████████████████████
  ██ CHALLENGE #4: CHANGE ANIMATION SPEED RESPONSE!                     ██
  ██ ➤ The animation speed is controlled by GSR changes                 ██
  ██ ➤ To make it more/less sensitive, modify the library's             ██
  ██   visualizeGSR() function speed mapping                            ██
  ████████████████████████████████████████████████████████████████████*/

  delay(10);
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                      SERIAL COMMUNICATION
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n') {
      /*████████████████████████████████████████████████████████████████████
      ██ CHALLENGE #5: ADD A CUSTOM COMMAND!                                ██
      ██ ➤ Add your own command here before the library processes it        ██
      ██ ➤ Example: if (inputBuffer == "BLINK") { // your code here }      ██
      ██ ➤ You can control LEDs, send data back, or trigger effects        ██
      ████████████████████████████████████████████████████████████████████*/

      // Special handling for calibration command
      if (inputBuffer == "CALIBRATE") {
        performCalibration();
      } else {
        // Let the library handle all other commands
        visualizer->processCommand(inputBuffer, emaValue, baseline);
      }
      inputBuffer = "";
    } else if (c != '\r') {
      inputBuffer += c;
    }
  }
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                          CALIBRATION
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void performCalibration() {
  visualizer->sendStatus("CALIBRATING");

  unsigned long startTime = millis();
  int sampleCount = 0;
  float sum = 0;

  // Reset min/max
  gsrMin = 1023;
  gsrMax = 0;

  // Collect samples for 5 seconds
  while (millis() - startTime < 5000) {
    int reading = analogRead(GSR_PIN);

    if (reading < gsrMin) gsrMin = reading;
    if (reading > gsrMax) gsrMax = reading;

    sum += reading;
    sampleCount++;

    // Update EMA during calibration
    if (emaValue == 0) {
      emaValue = reading;
    } else {
      emaValue = visualizer->applyExponentialFilter(reading, emaValue);
    }

    // Show calibration animation
    visualizer->showCalibrationAnimation();

    delay(20);
  }

  // Calculate baseline
  baseline = sum / sampleCount;
  gsrMin = max(0, gsrMin - 20);
  gsrMax = min(1023, gsrMax + 20);

  // Initialize processing variables
  visualizer->updateAdaptiveBaseline(emaValue);
  lastEmaValue = emaValue;

  // Success animation
  visualizer->flashSuccess();

  isCalibrated = true;
  visualizer->sendStatus("CALIBRATION_COMPLETE");
}

/*
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
                        P5.JS COMMUNICATION GUIDE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

COMMANDS FROM P5.JS:
───────────────────
• "CALIBRATE"           - Start calibration
• "RESET"               - Reset baseline
• "GROUP:3"             - Set group number (1-5)
• "sim"                 - Toggle simulation mode
• "{\"ema\":456.78}"    - Send simulated data
• "LED:OFF"             - Turn off LEDs
• "LED:GSR"             - GSR visualization
• "LED:RAINBOW"         - Rainbow effect
• "LED:PULSE"           - Pulsing effect
• "LED:COLOR:255,0,0"   - Set solid color (R,G,B)
• "BRIGHTNESS:100"      - Set brightness (0-255)
• "PING"                - Test connection

DATA TO P5.JS:
─────────────
• {"ema":value}         - Continuous GSR data
• {"status":"message"}  - Status updates

GROUP COLORS:
────────────
1: Red    2: Green    3: Blue    4: Orange    5: Purple

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
*/