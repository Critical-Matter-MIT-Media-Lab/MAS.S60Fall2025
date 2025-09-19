/*

 * GSR sensors measure skin conductance, which changes based on emotional arousal,
 * stress levels, or physiological responses. Higher conductance typically indicates
 * higher arousal or stress.
 *
 * Hardware Requirements:
 * - Arduino board (Uno, Nano, etc.)
 * - Seeed Grove GSR Sensor
 * - WS2812 LED Strip or NeoPixel (20 pixels)
 * - Appropriate power supply for LED strip (5V, sufficient current)
 *
 * Connections:
 * - GSR Sensor: Connect to analog pin A0
 * - WS2812 Strip: Connect data pin to digital pin 6
 * - Power: Ensure both components have proper 5V and GND connections
 */

#include <Adafruit_NeoPixel.h>  // Library for controlling WS2812 LED strips

// ============================================================================
// !!!HARDWARE CONFIGURATION!!!
// ============================================================================

// GSR Sensor Configuration
const int GSR_PIN = 2;  // Analog input pin for Grove GSR sensor
// WS2812 LED Strip Configuration
const int LED_PIN = 3;   // Digital pin connected to the LED strip's data input
                         // This pin sends the control signals to the LEDs
const int NUM_LEDS = 20; // Total number of LEDs in your strip

// ============================================================================
// NEOPIXEL INITIALIZATION
// ============================================================================

// Create NeoPixel object
// Parameters: (number of pixels, pin number, pixel type flags)
// NEO_GRB: Pixels are wired for GRB color order (most common)
// NEO_KHZ800: 800 KHz data transmission speed (for WS2812 LEDs)
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============================================================================
// GSR SENSOR VARIABLES
// ============================================================================

int gsrValue = 0;        // Current raw reading from the GSR sensor (0-1023)
                         // Arduino's ADC provides 10-bit resolution

int gsrMin = 1023;       // Minimum GSR value seen during calibration
                         // Initialized to maximum possible value (will decrease during calibration)

int gsrMax = 0;          // Maximum GSR value seen during calibration
                         // Initialized to minimum possible value (will increase during calibration)

// ============================================================================
// SIGNAL SMOOTHING VARIABLES
// ============================================================================
// Smoothing helps eliminate noise and sudden spikes in sensor readings

const int numReadings = 10;     // Number of readings to average
                                 // Higher = smoother but slower response
int readings[numReadings];      // Array to store recent readings (circular buffer)
int readIndex = 0;               // Current position in the readings array
int total = 0;                   // Running total of all readings
int average = 0;                 // Calculated average of recent readings

// ============================================================================
// ADVANCED FILTERING VARIABLES
// ============================================================================

// Exponential Moving Average (EMA) / Low-pass filter
// Provides additional smoothing, good for removing high-frequency noise
float alpha = 0.3;              // EMA coefficient (0.0 to 1.0, lower = more smoothing)
float filteredValue = 0;        // Current EMA filtered value

// Baseline calibration for relative measurements
float baseline = 0;             // Baseline GSR value (established during calibration)
float baselineFilteredValue = 0; // Baseline for filtered value

// ============================================================================
// AFFECT CALCULATION VARIABLES
// ============================================================================
// Long-term emotional state assessment

const int affectWindowSize = 3000;  // Number of samples for affect calculation (30 seconds at 10ms delay)
float affectBuffer[affectWindowSize];  // Circular buffer storing baseline-adjusted values
int affectBufferIndex = 0;          // Current index in affect buffer
unsigned long affectSampleCount = 0; // Total number of samples collected
unsigned long lastAffectUpdate = 0;  // Last time affect was updated
const unsigned long affectUpdateInterval = 5000; // Update affect every 5 seconds

// Affect results
float currentAffect = 0;        // Current affect value (long-term average excluding spikes)
int validSamplesInWindow = 0;   // Number of valid (non-spike) samples in current window
float affectTrend = 0;          // Trend direction of affect over time

// ============================================================================
// SPIKE DETECTION VARIABLES
// ============================================================================
// For artifact exclusion and signal quality monitoring
// Now optimized for EMA-based detection

float spikeThreshold = 100.0;   // Detect ~100 unit changes in EMA
const int spikeWindowMs = 300;  // Time window for spike detection (milliseconds)
float emaHistory[5];            // Store last 5 EMA values for trend analysis
int emaHistoryIndex = 0;        // Current position in EMA history
float emaBeforeSpike = 0;       // EMA value before spike detected
bool inSpike = false;           // Currently in a spike
unsigned long spikeStartTime = 0; // When spike started
const unsigned long maxSpikeDuration = 500; // Maximum spike duration in ms
unsigned long lastEmaTime = 0;  // Time of last EMA sample for rate calculation

// ============================================================================
// CALIBRATION VARIABLES
// ============================================================================
// Calibration helps establish the baseline range for each user/session

bool isCalibrated = false;              // Flag to track if calibration is complete
unsigned long calibrationStartTime = 0;  // Timestamp when calibration began
const unsigned long calibrationDuration = 5000; // Calibration period in milliseconds
const int calibrationReadings = 50;     // Number of readings to average for baseline

// ============================================================================
// SETUP FUNCTION - Runs once when Arduino starts
// ============================================================================
void setup() {
  // Initialize serial communication for debugging
  // 9600 baud rate is standard and reliable for most applications
  Serial.begin(9600);

  // ===== LED Strip Initialization =====
  strip.begin();           // Initialize the LED strip hardware
  strip.show();            // Update strip with data (turns all LEDs off initially)
  strip.setBrightness(50); // Set global brightness (0-255)
                           // Lower values = dimmer, higher = brighter
                           // 50 is a good balance for visibility without being blinding

  // ===== Initialize Smoothing Array =====
  // Fill the readings array with zeros to start with a clean slate
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  // ===== Initialize Affect Buffer =====
  for (int i = 0; i < affectWindowSize; i++) {
    affectBuffer[i] = 0;
  }

  // ===== Initialize EMA History for spike detection =====
  for (int i = 0; i < 5; i++) {
    emaHistory[i] = 0;
  }

  // ===== User Instructions =====
  Serial.println("=== GSR SENSOR WITH ADVANCED FILTERING ===");
  Serial.println("Starting 5-second calibration...");
  Serial.println("Please relax and remain still for accurate baseline");
  Serial.println("Calibrating baseline...");

  // Record when calibration starts
  calibrationStartTime = millis();

  // Perform calibration
  long calibrationSum = 0;
  int calibrationCount = 0;
  unsigned long startTime = millis();

  // Collect baseline readings for calibrationDuration milliseconds
  while (millis() - startTime < calibrationDuration) {
    int reading = analogRead(GSR_PIN);
    calibrationSum += reading;
    calibrationCount++;

    // Update filtered value during calibration
    filteredValue = alpha * reading + (1 - alpha) * filteredValue;

    // Update min/max during calibration
    if (reading < gsrMin) gsrMin = reading;
    if (reading > gsrMax) gsrMax = reading;

    // Show calibration animation
    showCalibrationAnimation();

    // Show progress
    if (calibrationCount % 10 == 0) {
      Serial.print(".");
    }

    delay(calibrationDuration / calibrationReadings);
  }

  // Calculate baseline as average of all calibration readings
  baseline = (float)calibrationSum / calibrationCount;
  baselineFilteredValue = filteredValue;

  // Initialize EMA history with baseline value
  for (int i = 0; i < 5; i++) {
    emaHistory[i] = filteredValue;
  }
  lastEmaTime = millis();

  // Add margin to min/max
  gsrMin = max(0, gsrMin - 20);
  gsrMax = min(1023, gsrMax + 20);

  Serial.println();
  Serial.println("\nCalibration complete!");
  Serial.print("Baseline value: ");
  Serial.println(baseline);
  Serial.print("GSR Range: ");
  Serial.print(gsrMin);
  Serial.print(" - ");
  Serial.println(gsrMax);
  Serial.println("\nStarting GSR monitoring...");
  Serial.println("Open Serial Plotter to visualize data");
  Serial.println("Format: Raw:value,Avg:value,EMA:value,Baseline:value,Affect:value\n");

  // Flash green LEDs to indicate successful calibration
  for (int i = 0; i < 3; i++) {
    setAllPixels(0, 255, 0);
    delay(200);
    strip.clear();
    strip.show();
    delay(200);
  }

  // Initialize timing for affect updates
  lastAffectUpdate = millis();
  isCalibrated = true;
}

// ============================================================================
// MAIN LOOP - Runs continuously after setup
// ============================================================================
void loop() {
  // Only run if calibration is complete
  if (!isCalibrated) {
    return;
  }

  // ===== Step 1: Read GSR Sensor =====
  gsrValue = analogRead(GSR_PIN);

  // ===== Step 2: Apply Moving Average Filter =====
  total = total - readings[readIndex];
  readings[readIndex] = gsrValue;
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  average = total / numReadings;

  // ===== Step 3: Apply Exponential Moving Average (EMA) =====
  // This provides additional smoothing on top of the moving average
  filteredValue = alpha * gsrValue + (1 - alpha) * filteredValue;

  // ===== Step 4: Calculate Baseline-Adjusted Value =====
  // Positive values = increase from baseline
  float baselineAdjusted = filteredValue - baseline;

  // ===== Step 5: Enhanced Spike Detection Based on EMA =====
  // Store current EMA in history buffer
  emaHistory[emaHistoryIndex] = filteredValue;
  emaHistoryIndex = (emaHistoryIndex + 1) % 5;

  // Calculate time since last sample
  unsigned long currentTime = millis();
  float timeDelta = currentTime - lastEmaTime;

  if (timeDelta > 0 && lastEmaTime > 0) {
    // Calculate rate of change in EMA over time window
    float oldestEma = emaHistory[(emaHistoryIndex + 1) % 5]; // Oldest value in circular buffer
    float emaChangeTotal = abs(filteredValue - oldestEma);

    // Check time window - if we have enough history
    if (timeDelta <= spikeWindowMs) {
      // Detect spike if EMA changed by ~100 units within the time window
      if (!inSpike && emaChangeTotal >= spikeThreshold) {
        inSpike = true;
        spikeStartTime = currentTime;
        emaBeforeSpike = oldestEma;

        // Debug output for spike detection
        Serial.print(" [SPIKE START: EMA changed ");
        Serial.print(emaChangeTotal);
        Serial.print(" units in ");
        Serial.print(timeDelta);
        Serial.print("ms]");
      }
    }

    // Check if spike should end
    if (inSpike) {
      unsigned long spikeDuration = currentTime - spikeStartTime;
      float recoveryAmount = abs(filteredValue - emaBeforeSpike);

      // End spike if:
      // 1. EMA has stabilized (change < 20% of threshold)
      // 2. Maximum spike duration exceeded
      if (recoveryAmount < spikeThreshold * 0.2 || spikeDuration > maxSpikeDuration) {
        inSpike = false;
        Serial.print(" [SPIKE END]");
      }
    }
  }

  lastEmaTime = currentTime;

  // ===== Step 6: Update Affect Buffer =====
  affectBuffer[affectBufferIndex] = baselineAdjusted;
  affectBufferIndex = (affectBufferIndex + 1) % affectWindowSize;
  affectSampleCount++;

  // ===== Step 7: Calculate Affect at Intervals =====
  if (currentTime - lastAffectUpdate >= affectUpdateInterval) {
    lastAffectUpdate = currentTime;
    calculateAffect();
  }

  // ===== Step 8: Visualize on LEDs =====
  // Use filtered value for smoother LED visualization
  visualizeGSRWithFilters(average, filteredValue, baselineAdjusted);

  // ===== Step 9: Print Plottable Data =====
  // Format optimized for Arduino Serial Plotter
  // Using labels for better identification in plotter

  // Group 1: Raw sensor values
  Serial.print("Raw:");
  Serial.print(gsrValue);
  Serial.print(",");
  Serial.print("Avg:");
  Serial.print(average);
  Serial.print(",");
  Serial.print("EMA:");
  Serial.print(filteredValue, 1);

  // Visual separator (double comma creates space in plotter)
  Serial.print(",,");

  // Group 2: Processed values
  Serial.print("Baseline:");
  Serial.print(baselineAdjusted, 1);
  Serial.print(",");
  Serial.print("Affect:");
  Serial.print(currentAffect, 1);

  // Add spike indicator as text (won't create new plot line)
  if (inSpike) {
    Serial.print(" [SPIKE DETECTED]");
  }

  Serial.println();

  // Control update rate - 10ms for 100Hz sampling
  delay(10);
}

// ============================================================================
// AFFECT CALCULATION FUNCTION
// ============================================================================
void calculateAffect() {
  float sum = 0;
  float sumSquares = 0;
  validSamplesInWindow = 0;
  int samplesToAnalyze = min((int)affectSampleCount, affectWindowSize);

  // Calculate mean
  for (int i = 0; i < samplesToAnalyze; i++) {
    sum += affectBuffer[i];
  }
  float mean = sum / samplesToAnalyze;

  // Calculate standard deviation
  for (int i = 0; i < samplesToAnalyze; i++) {
    float diff = affectBuffer[i] - mean;
    sumSquares += diff * diff;
  }
  float stdDev = sqrt(sumSquares / samplesToAnalyze);

  // Recalculate affect excluding outliers (values beyond 2 standard deviations)
  sum = 0;
  float previousAffect = currentAffect;

  for (int i = 0; i < samplesToAnalyze; i++) {
    if (abs(affectBuffer[i] - mean) <= 2 * stdDev) {
      sum += affectBuffer[i];
      validSamplesInWindow++;
    }
  }

  if (validSamplesInWindow > 0) {
    currentAffect = sum / validSamplesInWindow;

    // Calculate trend
    if (affectSampleCount > affectWindowSize) {
      affectTrend = currentAffect - previousAffect;
    }
  }

  // Don't print during normal operation to avoid disrupting plotter
  // Affect value is already shown in the main data stream
}

// ============================================================================
// ENHANCED GSR VISUALIZATION FUNCTION
// Maps filtered GSR readings to LED patterns
// ============================================================================
void visualizeGSRWithFilters(int avgValue, float emaValue, float baselineAdj) {
  // Use EMA value for smoother visualization
  int mappedValue = constrain(
    map((int)emaValue, gsrMin, gsrMax, 0, NUM_LEDS),
    0,
    NUM_LEDS
  );

  strip.clear();

  // Track EMA changes for dynamic effects
  static float lastEmaValue = emaValue;
  float emaChange = abs(emaValue - lastEmaValue);
  float changeIntensity = constrain(emaChange * 2, 0, 100); // Scale change for visibility

  // Color gradient with dynamic effects
  for (int i = 0; i < mappedValue; i++) {
    int r, g, b;

    if (inSpike) {
      // Flash red during spike
      if (millis() % 200 < 100) {
        r = 255; g = 0; b = 0;
      } else {
        r = 100; g = 0; b = 0;
      }
    } else {
      // Normal gradient with dynamic modulation based on EMA changes
      if (i < NUM_LEDS / 2) {
        r = 0;
        g = map(i, 0, NUM_LEDS/2, 0, 255);
        b = map(i, 0, NUM_LEDS/2, 255, 0);
      } else {
        r = map(i, NUM_LEDS/2, NUM_LEDS, 0, 255);
        g = map(i, NUM_LEDS/2, NUM_LEDS, 255, 0);
        b = 0;
      }

      // Add dynamic pulsing based on EMA change intensity
      // Larger changes = stronger/faster pulsing
      float pulseSpeed = 500.0 - (changeIntensity * 3); // Faster pulse with more change
      float pulse = (sin(millis() / pulseSpeed) + 1.0) / 2.0;
      float brightnessModulation = 0.6 + (0.4 * pulse);

      // Apply brightness modulation - more change = more variation
      if (changeIntensity > 5) {
        r = r * brightnessModulation;
        g = g * brightnessModulation;
        b = b * brightnessModulation;
      }
    }

    strip.setPixelColor(i, strip.Color(r, g, b));
  }

  // Enhanced breathing effect varies with EMA changes
  if (!inSpike && mappedValue > 0 && mappedValue < NUM_LEDS) {
    // Breathing speed varies with EMA changes
    float breathSpeed = 300.0 - (changeIntensity * 2); // Faster breathing with more change
    float breath = (sin(millis() / breathSpeed) + 1.0) / 2.0;
    int lastLed = mappedValue - 1;
    uint32_t color = strip.getPixelColor(lastLed);
    int r = ((color >> 16) & 0xFF) * (0.3 + 0.7 * breath); // More dramatic breathing
    int g = ((color >> 8) & 0xFF) * (0.3 + 0.7 * breath);
    int b = (color & 0xFF) * (0.3 + 0.7 * breath);
    strip.setPixelColor(lastLed, strip.Color(r, g, b));
  }

  // Add wave effect on significant changes (ripple through LEDs)
  if (changeIntensity > 15 && !inSpike && mappedValue > 3) {
    int wavePos = (millis() / 50) % mappedValue;
    uint32_t color = strip.getPixelColor(wavePos);
    int r = ((color >> 16) & 0xFF) + 50;
    int g = ((color >> 8) & 0xFF) + 50;
    int b = (color & 0xFF) + 50;
    strip.setPixelColor(wavePos, strip.Color(
      r > 255 ? 255 : r,
      g > 255 ? 255 : g,
      b > 255 ? 255 : b
    ));
  }

  lastEmaValue = emaValue;

  strip.show();
}

// ============================================================================
// ORIGINAL GSR VISUALIZATION FUNCTION (kept for compatibility)
// ============================================================================
void visualizeGSR(int gsrValue) {
  // ===== Map GSR to LED Count =====
  // Convert the GSR reading to the number of LEDs to light up
  // map() scales the GSR value from its calibrated range to 0-20 LEDs
  // constrain() ensures we never exceed the strip boundaries
  int mappedValue = constrain(
    map(gsrValue, gsrMin, gsrMax, 0, NUM_LEDS),
    0,
    NUM_LEDS
  );

  // Clear all LEDs to start fresh
  strip.clear();

  // ===== Light Up LEDs with Color Gradient =====
  // The gradient represents emotional states:
  // Blue (calm) -> Green (moderate) -> Red (stressed/excited)

  for (int i = 0; i < mappedValue; i++) {
    // Variables to store RGB color values
    int r, g, b;

    if (i < NUM_LEDS / 2) {
      // ===== First Half: Blue to Green Gradient =====
      // This represents the transition from calm to moderate arousal

      r = 0;  // No red component in blue-green range

      // Green increases from 0 to 255 as we move through first half
      g = map(i, 0, NUM_LEDS/2, 0, 255);

      // Blue decreases from 255 to 0 as we move through first half
      b = map(i, 0, NUM_LEDS/2, 255, 0);

    } else {
      // ===== Second Half: Green to Red Gradient =====
      // This represents the transition from moderate to high arousal

      // Red increases from 0 to 255 as we move through second half
      r = map(i, NUM_LEDS/2, NUM_LEDS, 0, 255);

      // Green decreases from 255 to 0 as we move through second half
      g = map(i, NUM_LEDS/2, NUM_LEDS, 255, 0);

      b = 0;  // No blue component in green-red range
    }

    // Set the calculated color for this LED
    strip.setPixelColor(i, strip.Color(r, g, b));
  }

  // ===== Add Breathing Effect to Last LED =====
  // This creates a pulsing effect on the last active LED for visual interest

  if (mappedValue > 0 && mappedValue < NUM_LEDS) {
    // Use sine wave to create smooth breathing animation
    // sin() output ranges from -1 to 1, we scale it to 0 to 1
    float breath = (sin(millis() / 200.0) + 1.0) / 2.0;

    int lastLed = mappedValue - 1;  // Index of last lit LED

    // Get the current color of the last LED
    uint32_t color = strip.getPixelColor(lastLed);

    // Extract individual RGB components from the packed color value
    // Color is stored as 32-bit: 0x00RRGGBB
    int r = (color >> 16) & 0xFF;  // Shift right 16 bits and mask
    int g = (color >> 8) & 0xFF;   // Shift right 8 bits and mask
    int b = color & 0xFF;          // Just mask the blue component

    // Apply breathing effect by modulating brightness
    // Varies between 50% and 100% brightness
    r = r * (0.5 + 0.5 * breath);
    g = g * (0.5 + 0.5 * breath);
    b = b * (0.5 + 0.5 * breath);

    // Update the LED with the breathing effect applied
    strip.setPixelColor(lastLed, strip.Color(r, g, b));
  }

  // Send the updated colors to the LED strip
  strip.show();
}

// ============================================================================
// CALIBRATION ANIMATION FUNCTION
// Shows a pulsing white animation during the calibration phase
// ============================================================================
void showCalibrationAnimation() {
  // Create a smooth pulsing effect using sine wave
  // millis()/300.0 controls the speed of pulsing (lower = faster)
  float pulse = (sin(millis() / 300.0) + 1.0) / 2.0; // Normalized to 0-1

  // Calculate brightness: varies between 20 (dim) and 70 (moderate)
  // This creates a gentle pulsing effect that's not too bright
  int brightness = 20 + (50 * pulse);

  // Apply the same white color to all LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    // White is created by equal amounts of R, G, and B
    strip.setPixelColor(i, strip.Color(brightness, brightness, brightness));
  }

  // Update the LED strip with the new colors
  strip.show();
}

// ============================================================================
// UTILITY FUNCTION: Set All Pixels
// Helper function to set all LEDs to the same color
// ============================================================================
void setAllPixels(int r, int g, int b) {
  // Loop through all LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  // Update the strip to show the new colors
  strip.show();
}