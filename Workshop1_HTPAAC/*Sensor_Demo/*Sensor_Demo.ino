
const int analogInPin = 9;  // Analog input pin that the GSR sensor is attached to

int sensorValue = 0;  // value read from the GSR sensor
int outputValue = 0;  // value output to the PWM (analog out)

// Filter variables
const int numReadings = 10;  // number of readings for moving average
int readings[numReadings];   // array to store readings
int readIndex = 0;           // index of current reading
int total = 0;               // running total
int average = 0;             // average value

// Low-pass filter coefficient (0.0 to 1.0, lower = more smoothing)
float alpha = 0.3;
float filteredValue = 0;

// Baseline calibration variables
const int calibrationTime = 5000;  // Calibration time in milliseconds (5 seconds)
const int calibrationReadings = 50;  // Number of readings to average for baseline
float baseline = 0;  // Baseline value
float baselineFilteredValue = 0;  // Baseline for filtered value
bool isCalibrated = false;  // Flag to indicate calibration complete

// Affect calculation - long-term emotional state assessment
const int affectWindowSize = 3000;  // Number of samples for affect calculation (30 seconds at 10ms delay)
float affectBuffer[affectWindowSize];  // Circular buffer storing baseline-adjusted values
int affectBufferIndex = 0;  // Current index in affect buffer
unsigned long affectSampleCount = 0;  // Total number of samples collected
unsigned long lastAffectUpdate = 0;  // Last time affect was updated
const unsigned long affectUpdateInterval = 5000;  // Update affect every 5 seconds

// Spike detection for artifact exclusion
float spikeThreshold = 15.0;  // Threshold for spike detection (adjustable based on sensor sensitivity)
float spikeRecoveryRate = 0.7;  // How much the signal must recover (70% back to pre-spike level)
float preSpikeValue = 0;  // Value before spike detected
bool inSpike = false;  // Currently in a spike
unsigned long spikeStartTime = 0;  // When spike started
const unsigned long maxSpikeDuration = 500;  // Maximum spike duration in ms

// Affect results
float currentAffect = 0;  // Current affect value (long-term average excluding spikes)
int validSamplesInWindow = 0;  // Number of valid (non-spike) samples in current window
float affectTrend = 0;  // Trend direction of affect over time

void setup() {
  // initialize serial communications at 9600 bps:
  Serial.begin(9600);

  // Initialize all readings to 0
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }

  // Calibration process
  Serial.println("=== GSR SENSOR CALIBRATION ===");
  Serial.println("Please relax and remain still for 5 seconds...");
  Serial.println("Calibrating baseline...");

  long calibrationSum = 0;
  int calibrationCount = 0;
  unsigned long startTime = millis();

  // Collect baseline readings for calibrationTime milliseconds
  while (millis() - startTime < calibrationTime) {
    int reading = analogRead(analogInPin);
    calibrationSum += reading;
    calibrationCount++;

    // Update filtered value during calibration
    filteredValue = alpha * reading + (1 - alpha) * filteredValue;

    // Show progress
    if (calibrationCount % 10 == 0) {
      Serial.print(".");
    }

    delay(calibrationTime / calibrationReadings);
  }

  // Calculate baseline as average of all calibration readings
  baseline = (float)calibrationSum / calibrationCount;
  baselineFilteredValue = filteredValue;

  Serial.println();
  Serial.print("Calibration complete! Baseline value: ");
  Serial.println(baseline);
  Serial.println("Starting GSR monitoring with baseline correction...");
  Serial.println("Format: Raw:value,MovingAvg:value,EMA:value,Baseline-Adjusted:value,Affect:value");
  Serial.println("=================================");

  // Initialize affect buffer
  for (int i = 0; i < affectWindowSize; i++) {
    affectBuffer[i] = 0;
  }

  // Initialize timing for affect updates
  lastAffectUpdate = millis();

  isCalibrated = true;
}

void loop() {
  // Only run if calibration is complete
  if (!isCalibrated) {
    return;
  }

  // read the analog in value:
  sensorValue = analogRead(analogInPin);

  // Moving Average Filter
  // subtract the last reading
  total = total - readings[readIndex];
  // read from the sensor
  readings[readIndex] = sensorValue;
  // add the reading to the total
  total = total + readings[readIndex];
  // advance to the next position in the array
  readIndex = readIndex + 1;

  // if we're at the end of the array, wrap around to the beginning
  if (readIndex >= numReadings) {
    readIndex = 0;
  }

  // calculate the average
  average = total / numReadings;

  // Exponential Moving Average (EMA) / Low-pass filter
  // This provides additional smoothing on top of the moving average
  filteredValue = alpha * sensorValue + (1 - alpha) * filteredValue;

  // Calculate baseline-adjusted value (positive values = increase from baseline)
  float baselineAdjusted = filteredValue - baseline;

  // Simple spike detection based on rate of change
  static float lastValue = 0;
  float changeRate = abs(baselineAdjusted - lastValue);

  if (!inSpike) {
    // Check if we're entering a spike (rapid change)
    if (changeRate > spikeThreshold) {
      inSpike = true;
      spikeStartTime = millis();
      preSpikeValue = lastValue;
    }
  } else {
    // Check if spike has ended (either returned to near pre-spike level or timed out)
    unsigned long spikeDuration = millis() - spikeStartTime;
    float recoveryTarget = preSpikeValue + (baselineAdjusted - preSpikeValue) * (1 - spikeRecoveryRate);

    if (abs(baselineAdjusted - preSpikeValue) < spikeThreshold * 0.5 || spikeDuration > maxSpikeDuration) {
      inSpike = false;
    }
  }

  lastValue = baselineAdjusted;

  // Always add to buffer, but mark whether it's valid
  affectBuffer[affectBufferIndex] = baselineAdjusted;
  affectBufferIndex = (affectBufferIndex + 1) % affectWindowSize;
  affectSampleCount++;

  // Calculate affect only at specified intervals (every 5 seconds)
  unsigned long currentTime = millis();
  if (currentTime - lastAffectUpdate >= affectUpdateInterval) {
    lastAffectUpdate = currentTime;

    // Calculate affect from the entire buffer (up to 30 seconds of data)
    float sum = 0;
    float sumSquares = 0;
    validSamplesInWindow = 0;
    int samplesToAnalyze = min((int)affectSampleCount, affectWindowSize);

    // Calculate mean and identify outliers
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

      // Calculate trend (change in affect over time)
      if (affectSampleCount > affectWindowSize) {
        affectTrend = currentAffect - previousAffect;
      }
    }

    // Print affect update
    Serial.print("=== AFFECT UPDATE: ");
    Serial.print(currentAffect);
    Serial.print(" (");
    Serial.print(validSamplesInWindow);
    Serial.print("/");
    Serial.print(samplesToAnalyze);
    Serial.print(" valid samples, trend: ");
    Serial.print(affectTrend > 0 ? "+" : "");
    Serial.print(affectTrend);
    Serial.println(") ===");
  }

  // Print real-time values (affect is shown separately every 5 seconds)
  Serial.print("Raw:");
  Serial.print(sensorValue);
  Serial.print(",MovingAvg:");
  Serial.print(average);
  Serial.print(",EMA:");
  Serial.print(filteredValue);
  Serial.print(",BaselineAdj:");
  Serial.print(baselineAdjusted);
  if (inSpike) {
    Serial.print(",SPIKE");
  }
  Serial.println();

  // wait 10 milliseconds before the next loop
  // Increased delay for better GSR signal stability
  delay(10);
}
