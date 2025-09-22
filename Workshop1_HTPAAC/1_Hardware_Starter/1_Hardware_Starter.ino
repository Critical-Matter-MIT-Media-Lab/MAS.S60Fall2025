/*
╔══════════════════════════════════════════════════════════════════════════╗
║                    GSR SENSOR + LED STRIP VISUALIZER                      ║
╚══════════════════════════════════════════════════════════════════════════╝

 WHAT IS GSR?
 ------------
 GSR measures skin conductance, which changes with emotional arousal,
 stress, or excitement. Higher conductance = higher arousal/stress.

 HARDWARE CONNECTIONS:
 --------------------
 • GSR Sensor → Analog Pin A0 (GPIO 2 for ESP32)
 • LED Strip → Digital Pin 6 (GPIO 3 for ESP32)
 • Power → 5V and GND to both components

 FUNCTIONALITY:
 -------------
 1. Calibrates baseline GSR (5 seconds)
 2. Reads & filters GSR data
 3. Visualizes on LEDs (blue=calm → red=stressed)
 4. Outputs data to Serial Plotter

 ABOUT THIS CODE:
 ---------------
 This simplified version uses our custom GSRVisualizer library to keep the
 main code clean and readable. The library handles all complex signal
 processing, LED animations, and filtering algorithms, allowing you to
 focus on understanding the core GSR sensing concepts.

 Library files: GSRVisualizer.h and GSRVisualizer.cpp
*/

#include <Adafruit_NeoPixel.h>
#include "GSRVisualizer.h"  // Our custom library for clean, modular code

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         HARDWARE CONFIGURATION
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Pin Definitions
const int GSR_PIN = 2;     // Analog pin for GSR sensor (A0 on Arduino, GPIO on ESP32)
const int LED_PIN = 3;     // Digital pin for LED strip data
const int NUM_LEDS = 20;   // Number of LEDs in your strip

// Create LED strip object
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Create visualizer object
GSRVisualizer* visualizer;

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                          GLOBAL VARIABLES
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

//──────── Sensor State ────────
int gsrValue = 0;          // Current raw sensor reading
int gsrMin = 1023;         // Minimum value (for calibration)
int gsrMax = 0;            // Maximum value (for calibration)
bool isCalibrated = false; // Has calibration completed?

//──────── Moving Average Filter ────────
const int numReadings = 10;
int readings[numReadings];
int readIndex = 0;
int total = 0;
int average = 0;

//──────── Advanced Filters ────────
float filteredValue = 0;        // Exponentially smoothed value
float baseline = 0;             // Your personal baseline
float baselineAdjusted = 0;     // Deviation from baseline

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                            SETUP FUNCTION
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void setup() {
  // Initialize serial communication (for debugging)
  Serial.begin(9600);

  // Initialize LED strip
  strip.begin();

  /*████████████████████████████████████████████████████████████████████
  ██ CHALLENGE #1: CHANGE LED BRIGHTNESS!                              ██
  ██ ➤ Try values: 10 (dim), 50 (medium), 100 (bright), 255 (maximum) ██
  ████████████████████████████████████████████████████████████████████*/
  strip.setBrightness(50);  // Set brightness (0-255)

  strip.show();              // Turn off all LEDs initially

  // Initialize visualizer
  visualizer = new GSRVisualizer(strip, 10);

  // Initialize arrays
  initializeArrays();

  // Perform calibration (establishes your baseline)
  performCalibration();
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                             MAIN LOOP
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void loop() {
  // Wait for calibration to complete
  if (!isCalibrated) {
    return;
  }

  // 1. READ SENSOR
  gsrValue = analogRead(GSR_PIN);

  // 2. APPLY FILTERS
  average = visualizer->calculateMovingAverage(gsrValue);

  // Use responsive exponential filtering
  visualizer->setExponentialAlpha(0.5);  // Very responsive to changes
  filteredValue = visualizer->applyExponentialFilter(gsrValue, filteredValue);

  // Direct baseline adjustment - no amplification to see raw sensitivity
  baselineAdjusted = filteredValue - baseline;

  // 3. DETECT ANOMALIES (optional - for advanced users)
  visualizer->checkForSpikes(filteredValue);

  // 4. UPDATE LED VISUALIZATION
  visualizer->updateLEDDisplay(filteredValue, gsrMin, gsrMax);

  // 5. OUTPUT DATA FOR PLOTTING
  outputSerialData();

  // 6. CONTROL SAMPLING RATE
  /*████████████████████████████████████████████████████████████████████
  ██ CHALLENGE #2: ADJUST SAMPLING SPEED!                              ██
  ██ ➤ delay(5) = 200Hz (very fast)                                    ██
  ██ ➤ delay(10) = 100Hz (current)                                     ██
  ██ ➤ delay(20) = 50Hz (smooth)                                       ██
  ██ ➤ delay(50) = 20Hz (very smooth)                                  ██
  ████████████████████████████████████████████████████████████████████*/
  delay(10);  // 100Hz sampling (adjust as needed)
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         CORE FUNCTIONS
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Initialize all arrays with zeros
void initializeArrays() {
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }
}

// Simple calibration process
void performCalibration() {

  unsigned long startTime = millis();
  int sampleCount = 0;
  float sum = 0;

  /*████████████████████████████████████████████████████████████████████
  ██ CHALLENGE #3: CHANGE CALIBRATION TIME!                            ██
  ██ ➤ 3000 = 3 seconds (quick calibration)                            ██
  ██ ➤ 5000 = 5 seconds (current, balanced)                            ██
  ██ ➤ 10000 = 10 seconds (more accurate baseline)                     ██
  ████████████████████████████████████████████████████████████████████*/
  // Collect samples for 5 seconds
  while (millis() - startTime < 5000) {
    int reading = analogRead(GSR_PIN);

    // Track min/max for range
    if (reading < gsrMin) gsrMin = reading;
    if (reading > gsrMax) gsrMax = reading;

    // Calculate baseline
    sum += reading;
    sampleCount++;

    // Update initial filtered value
    if (filteredValue == 0) {
      filteredValue = reading;
    } else {
      filteredValue = 0.3 * reading + 0.7 * filteredValue;
    }

    // Show progress animation
    visualizer->showCalibrationAnimation();


    delay(20);
  }

  // Calculate results
  baseline = sum / sampleCount;
  gsrMin = max(0, gsrMin - 20);      // Add margin
  gsrMax = min(1023, gsrMax + 20);   // Add margin


  // Success animation
  visualizer->flashSuccess();

  isCalibrated = true;
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                      SIGNAL PROCESSING
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Calculate moving average for smoothing
int calculateMovingAverage(int newReading) {
  // Remove oldest reading from total
  total = total - readings[readIndex];

  // Add new reading
  readings[readIndex] = newReading;
  total = total + readings[readIndex];

  // Advance to next position
  readIndex = (readIndex + 1) % numReadings;

  // Return average
  return total / numReadings;
}

// Apply exponential smoothing filter
float applyExponentialFilter(int newReading) {
  const float alpha = 0.3;  // Smoothing factor (0-1, lower = smoother)
  return alpha * newReading + (1 - alpha) * filteredValue;
}

//──────── Spike Detection Variables ────────
bool inSpike = false;

/*████████████████████████████████████████████████████████████████████
██ CHALLENGE #4: ADJUST SPIKE SENSITIVITY!                            ██
██ ➤ 50.0 = Very sensitive (detects small movements)                  ██
██ ➤ 100.0 = Normal (current setting)                                 ██
██ ➤ 200.0 = Less sensitive (only large movements)                    ██
████████████████████████████████████████████████████████████████████*/
float spikeThreshold = 100.0;

float lastFilteredValue = 0;

// Check for sudden spikes in the signal
void checkForSpikes() {
  float change = abs(filteredValue - lastFilteredValue);

  if (change > spikeThreshold && !inSpike) {
    inSpike = true;

    // Flash LEDs red during spike
    for (int i = 0; i < NUM_LEDS; i++) {
      if (millis() % 200 < 100) {
        strip.setPixelColor(i, strip.Color(255, 0, 0));
      }
    }
    strip.show();
  } else if (change < spikeThreshold * 0.2 && inSpike) {
    inSpike = false;
  }

  lastFilteredValue = filteredValue;
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         DATA OUTPUT
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Output data for Serial Plotter
void outputSerialData() {
  // Format: Label:Value for each data stream
  Serial.print("Raw:");
  Serial.print(gsrValue);
  Serial.print(",");

  Serial.print("Average:");
  Serial.print(average);
  Serial.print(",");

  Serial.print("Filtered:");
  Serial.print(filteredValue, 1);
  Serial.print(",");

  Serial.print("Baseline_Adj:");
  Serial.print(baselineAdjusted, 1);

  Serial.println();
}

//────────────────────────────────────────────────────────────────────────
// Note: All complex signal processing and LED functions are now in the
// GSRVisualizer library. See GSRVisualizer.h and GSRVisualizer.cpp
//────────────────────────────────────────────────────────────────────────

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                    OPTIONAL ADVANCED FEATURES
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Uncomment #define below to enable:
// • Long-term affect calculation
// • Statistical outlier detection
// • Adaptive baseline adjustment
// • Pattern recognition

// Uncomment this line to enable advanced features:
// #define ENABLE_ADVANCED_FEATURES

#ifdef ENABLE_ADVANCED_FEATURES

// Advanced filter variables
const int affectWindowSize = 3000;
float affectBuffer[affectWindowSize];
int affectBufferIndex = 0;
float currentAffect = 0;
unsigned long lastAffectUpdate = 0;

// Calculate long-term emotional state
void calculateAffect() {
  // This function would calculate a 30-second rolling average
  // of your emotional state, filtering out temporary spikes
  //
  // Implementation:
  // 1. Collect samples over 30 seconds
  // 2. Remove statistical outliers
  // 3. Calculate mean and standard deviation
  // 4. Return normalized affect value
  //
  // Full implementation available in advanced version
}

// Apply statistical filtering
float applyStatisticalFilter(float value) {
  // This would use standard deviation to identify and
  // remove statistical outliers from the data
  //
  // Algorithm:
  // 1. Calculate mean of recent samples
  // 2. Calculate standard deviation
  // 3. Remove values > 2 std dev from mean
  // 4. Return filtered value
  //
  // Full implementation available in advanced version
  return value;
}

// Adaptive baseline adjustment
void updateAdaptiveBaseline() {
  // Slowly adjusts baseline to account for long-term changes
  // such as skin moisture, temperature, or sensor drift
  //
  // baseline = baseline * 0.999 + currentValue * 0.001
}

// Pattern recognition for meditation detection
bool detectMeditationState() {
  // Analyzes GSR patterns to detect meditation or relaxation
  //
  // Looks for:
  // - Decreasing baseline
  // - Reduced variance
  // - Slower oscillations
  // - Absence of spikes
  //
  return false;
}

#endif // ENABLE_ADVANCED_FEATURES

/*
╔══════════════════════════════════════════════════════════════════════════╗
║                      EXERCISES & CHALLENGES                               ║
╠══════════════════════════════════════════════════════════════════════════╣
║ BEGINNER:                                                                ║
║ • Change LED brightness (line 74)                                       ║
║ • Adjust sampling rate (line 118)                                       ║
║ • Modify color gradients                                                ║
╠══════════════════════════════════════════════════════════════════════════╣
║ INTERMEDIATE:                                                            ║
║ • Add buzzer for stress feedback                                        ║
║ • Create custom LED patterns                                            ║
║ • Log data to SD card                                                   ║
╠══════════════════════════════════════════════════════════════════════════╣
║ ADVANCED:                                                                ║
║ • Add Bluetooth connectivity                                            ║
║ • Build meditation trainer                                              ║
║ • Implement pattern recognition                                         ║
╚══════════════════════════════════════════════════════════════════════════╝
*/

/*
┌──────────────────────────────────────────────────────────────────────────┐
│                         TROUBLESHOOTING                                  │
├──────────────────────────────────────────────────────────────────────────┤
│ No LED response → Check pins, verify 5V power                           │
│ Noisy readings → Clean sensor, moisten fingers                          │
│ Values stuck → Check GSR_PIN configuration                              │
│ LEDs too bright/dim → Adjust setBrightness() value                      │
│ Serial Plotter issues → Close Serial Monitor, check baud rate           │
└──────────────────────────────────────────────────────────────────────────┘
*/


// END OF CODE