/*
╔══════════════════════════════════════════════════════════════════════════╗
║                    GSR VISUALIZER IMPLEMENTATION                          ║
╚══════════════════════════════════════════════════════════════════════════╝
*/

#include "GSRVisualizer.h"

GSRVisualizer::GSRVisualizer(Adafruit_NeoPixel& ledStrip, int numSamples)
    : strip(ledStrip), numReadings(numSamples), readIndex(0), total(0),
      alpha(0.3), inSpike(false), spikeThreshold(100.0), lastFilteredValue(0),
      animationPosition(0), lastAnimationTime(0), trailLength(5),
      currentLEDMode(MODE_GSR_VISUALIZATION), solidColor(0), groupNumber(1),
      adaptiveBaseline(0), adaptiveAlpha(0.001), normalizedEma(0),
      shortTermBaseline(0), simulationMode(false), simulatedEma(0) {

    numLeds = strip.numPixels();
    readings = new int[numReadings];

    for (int i = 0; i < numReadings; i++) {
        readings[i] = 0;
    }

    // Initialize group colors
    groupColors = new GroupColor[5];
    groupColors[0] = {255, 0, 0};     // Red
    groupColors[1] = {0, 255, 0};     // Green
    groupColors[2] = {0, 0, 255};     // Blue
    groupColors[3] = {255, 128, 0};   // Orange
    groupColors[4] = {255, 0, 255};   // Purple
}

GSRVisualizer::~GSRVisualizer() {
    delete[] readings;
    delete[] groupColors;
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         SIGNAL PROCESSING
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

int GSRVisualizer::calculateMovingAverage(int newReading) {
    total = total - readings[readIndex];
    readings[readIndex] = newReading;
    total = total + readings[readIndex];
    readIndex = (readIndex + 1) % numReadings;
    return total / numReadings;
}

float GSRVisualizer::applyExponentialFilter(int newReading, float currentFiltered) {
    return alpha * newReading + (1 - alpha) * currentFiltered;
}

void GSRVisualizer::checkForSpikes(float filteredValue) {
    float change = abs(filteredValue - lastFilteredValue);

    // Dynamic threshold - more sensitive to changes
    float dynamicThreshold = spikeThreshold * 0.5;  // More sensitive

    if (change > dynamicThreshold && !inSpike) {
        inSpike = true;

        // Create a bright flash effect when spike detected
        static unsigned long spikeStartTime = millis();
        spikeStartTime = millis();

        // Bright white flash that fades to red
        for (int i = 0; i < numLeds; i++) {
            strip.setPixelColor(i, strip.Color(255, 100, 100));  // Bright pink-white
        }
        strip.show();

    } else if (change < dynamicThreshold * 0.3 && inSpike) {
        inSpike = false;
    }

    lastFilteredValue = filteredValue;
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                      ADVANCED PROCESSING
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void GSRVisualizer::updateAdaptiveBaseline(float emaValue) {
    if (adaptiveBaseline == 0) {
        adaptiveBaseline = emaValue;
    } else {
        adaptiveBaseline = adaptiveBaseline * (1 - adaptiveAlpha) + emaValue * adaptiveAlpha;
    }
}

float GSRVisualizer::calculateNormalizedEma(float emaValue, int gsrMin, int gsrMax) {
    float deviation = emaValue - adaptiveBaseline;
    float range = max(50.0, abs(gsrMax - gsrMin) * 0.3);
    normalizedEma = constrain((deviation / range) + 0.5, 0, 1);
    return normalizedEma;
}

float GSRVisualizer::getCombinedSignal(float emaValue, float emaDerivative, int gsrMin, int gsrMax) {
    // Update short-term baseline
    if (shortTermBaseline == 0) {
        shortTermBaseline = emaValue;
    } else {
        shortTermBaseline = shortTermBaseline * 0.98 + emaValue * 0.02;
    }

    float shortTermDeviation = (emaValue - shortTermBaseline) / max(20.0, abs(gsrMax - gsrMin) * 0.1);

    // Combine signals
    float combinedSignal = normalizedEma * 0.4 +
                          (emaDerivative / 10.0 + 0.5) * 0.3 +
                          (shortTermDeviation + 0.5) * 0.3;

    combinedSignal = constrain(combinedSignal, 0, 1);

    // Amplify small changes
    return pow(combinedSignal, 0.7);
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         BASIC LED VISUALIZATION
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void GSRVisualizer::updateLEDDisplay(float value, int gsrMin, int gsrMax) {
    // CRITICAL: GSR typically varies by ~100 units during normal use
    // We need to be very sensitive to these small changes

    // Calculate the actual change from baseline (expected range: -50 to +50)
    static float baselineValue = value;  // Initialize baseline on first call
    static bool firstCall = true;
    if (firstCall) {
        baselineValue = value;
        firstCall = false;
    }

    // Slowly adapt baseline (very slowly to catch real changes)
    baselineValue = baselineValue * 0.999 + value * 0.001;

    // The actual deviation from baseline (typically -50 to +50)
    float deviation = value - baselineValue;

    // Map the small deviation to speed (super sensitive)
    // Even a change of 10 units should be noticeable
    // Deviation of -50 to +50 maps to speed of 0.5 to 15 LEDs/second
    float flowSpeed = 3.0 + (deviation / 10.0);  // Every 10 units = 1 LED/sec faster
    flowSpeed = constrain(flowSpeed, 0.5, 15.0);

    // Animation variables
    static float flowPosition = 0;
    static unsigned long lastFlowTime = millis();
    unsigned long currentTime = millis();
    float deltaTime = (currentTime - lastFlowTime) / 1000.0;
    lastFlowTime = currentTime;

    // Update flow position based on speed
    flowPosition += flowSpeed * deltaTime;

    // Loop the flow position
    if (flowPosition >= numLeds) {
        flowPosition -= numLeds;
    }

    strip.clear();

    // Deep Pink color #FF1493 = RGB(255, 20, 147) for basic mode
    // Hard mode uses group colors for advanced visualization
    int baseR = 255;
    int baseG = 20;
    int baseB = 147;

    // Create a simple moving dot with trail
    int dotLength = 5;  // Length of the moving dot

    for (int i = 0; i < numLeds; i++) {
        // Calculate distance from the dot center
        float distance = abs(i - flowPosition);

        // Handle wrapping around the strip
        if (distance > numLeds / 2) {
            distance = numLeds - distance;
        }

        // Calculate intensity based on distance from dot center
        float intensity = 0;
        if (distance < dotLength) {
            intensity = 1.0 - (distance / dotLength);
            // No squaring - keep it linear for clarity
        }

        // Apply the color with intensity
        int r = baseR * intensity;
        int g = baseG * intensity;
        int b = baseB * intensity;

        strip.setPixelColor(i, strip.Color(r, g, b));
    }

    // Add a dim background glow so you can see the strip is on
    for (int i = 0; i < numLeds; i++) {
        uint32_t currentColor = strip.getPixelColor(i);
        if (currentColor == 0) {  // Only add glow to unlit pixels
            strip.setPixelColor(i, strip.Color(10, 1, 5));  // Very dim pink glow
        }
    }

    strip.show();
}

/*
╔══════════════════════════════════════════════════════════════════════════╗
║                        HARD MODE IMPLEMENTATIONS                          ║
║━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━║
║  The following implementations are specifically for Hard Mode with        ║
║  Web Serial communication and advanced LED animations                     ║
╚══════════════════════════════════════════════════════════════════════════╝
*/

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//              HARD MODE: LED MODE CONTROLLER
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void GSRVisualizer::updateLEDs(float emaValue, int gsrMin, int gsrMax, float emaDerivative) {
    switch (currentLEDMode) {
        case MODE_GSR_VISUALIZATION:
            visualizeGSR(false, emaValue, gsrMin, gsrMax, emaDerivative);
            break;

        case MODE_GSR_DOWNSTREAM:
            visualizeGSR(true, emaValue, gsrMin, gsrMax, emaDerivative);
            break;

        case MODE_SOLID_COLOR:
            setSolidColor();
            break;

        case MODE_PULSE:
            showPulse();
            break;

        case MODE_RAINBOW:
            showRainbow();
            break;

        case MODE_OFF:
            strip.clear();
            strip.show();
            break;
    }
}

uint32_t GSRVisualizer::getColorForLevel(int ledIndex) {
    // Using single color #FF1493 (Deep Pink) for all LEDs
    // RGB(255, 20, 147)
    return strip.Color(255, 20, 147);
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//         HARD MODE: ADVANCED GSR VISUALIZATION WITH TRAIL EFFECT
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void GSRVisualizer::visualizeGSR(bool downstream, float emaValue, int gsrMin, int gsrMax, float emaDerivative) {
    float animationSpeed;

    if (simulationMode && downstream) {
        animationSpeed = map(simulatedEma, 0, 1023, 0.5, 8.0);
    } else {
        updateAdaptiveBaseline(emaValue);
        calculateNormalizedEma(emaValue, gsrMin, gsrMax);
        float amplifiedSignal = getCombinedSignal(emaValue, emaDerivative, gsrMin, gsrMax);
        animationSpeed = 0.2 + amplifiedSignal * 7.8;
    }

    unsigned long currentTime = millis();
    float deltaTime = (currentTime - lastAnimationTime) / 1000.0;
    lastAnimationTime = currentTime;

    if (downstream) {
        animationPosition -= animationSpeed * deltaTime * 2;
        if (animationPosition < -trailLength) {
            animationPosition = numLeds + trailLength;
        }
    } else {
        animationPosition += animationSpeed * deltaTime * 2;
        if (animationPosition > numLeds + trailLength) {
            animationPosition = -trailLength;
        }
    }

    strip.clear();

    GroupColor baseColor = groupColors[constrain(groupNumber - 1, 0, 4)];

    for (int i = 0; i < numLeds; i++) {
        float distance = downstream ? (i - animationPosition) : (animationPosition - i);

        if (distance >= 0 && distance <= trailLength) {
            float intensity = 1.0 - (distance / trailLength);
            intensity = intensity * intensity;

            int r = baseColor.r * intensity;
            int g = baseColor.g * intensity;
            int b = baseColor.b * intensity;

            strip.setPixelColor(i, strip.Color(r, g, b));
        } else {
            float glowIntensity = map(emaValue, gsrMin, gsrMax, 0.02, 0.15);

            int r = baseColor.r * glowIntensity;
            int g = baseColor.g * glowIntensity;
            int b = baseColor.b * glowIntensity;

            strip.setPixelColor(i, strip.Color(r, g, b));
        }
    }

    strip.show();
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         BASIC LED ANIMATIONS
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void GSRVisualizer::showCalibrationAnimation() {
    float pulse = (sin(millis() / 300.0) + 1.0) / 2.0;
    int brightness = 20 + (30 * pulse);

    for (int i = 0; i < numLeds; i++) {
        strip.setPixelColor(i, strip.Color(brightness, brightness, brightness));
    }
    strip.show();
}

void GSRVisualizer::flashSuccess() {
    for (int i = 0; i < 3; i++) {
        setAllPixels(0, 255, 0);
        delay(200);
        strip.clear();
        strip.show();
        delay(200);
    }
}

void GSRVisualizer::setAllPixels(int r, int g, int b) {
    for (int i = 0; i < numLeds; i++) {
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
}

void GSRVisualizer::addBreathingEffect(int ledIndex) {
    float breath = (sin(millis() / 200.0) + 1.0) / 2.0;
    uint32_t color = strip.getPixelColor(ledIndex);

    int r = ((color >> 16) & 0xFF) * (0.5 + 0.5 * breath);
    int g = ((color >> 8) & 0xFF) * (0.5 + 0.5 * breath);
    int b = (color & 0xFF) * (0.5 + 0.5 * breath);

    strip.setPixelColor(ledIndex, strip.Color(r, g, b));
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//              HARD MODE: SPECIAL LED EFFECTS
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void GSRVisualizer::setSolidColor() {
    for (int i = 0; i < numLeds; i++) {
        strip.setPixelColor(i, solidColor);
    }
    strip.show();
}

void GSRVisualizer::showPulse() {
    float pulse = (sin(millis() / 300.0) + 1.0) / 2.0;
    int brightness = 20 + (200 * pulse);

    for (int i = 0; i < numLeds; i++) {
        strip.setPixelColor(i, strip.Color(brightness, brightness, brightness));
    }
    strip.show();
}

void GSRVisualizer::showRainbow() {
    static uint16_t hue = 0;

    for (int i = 0; i < numLeds; i++) {
        int pixelHue = hue + (i * 65536L / numLeds);
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }

    strip.show();
    hue += 256;
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//              HARD MODE: WEB SERIAL COMMUNICATION
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void GSRVisualizer::sendDataToP5(float emaValue) {
    Serial.print("{\"ema\":");
    Serial.print(emaValue, 2);
    Serial.println("}");
}

void GSRVisualizer::sendStatus(String status) {
    Serial.print("{\"status\":\"");
    Serial.print(status);
    Serial.println("\"}");
}

void GSRVisualizer::processCommand(String command, float& emaValue, float& baseline) {
    command.trim();

    if (command == "CALIBRATE") {
        sendStatus("CALIBRATION_REQUESTED");

    } else if (command == "RESET") {
        baseline = emaValue;
        adaptiveBaseline = emaValue;
        shortTermBaseline = emaValue;
        sendStatus("RESET_COMPLETE");

    } else if (command.startsWith("LED:")) {
        String ledCmd = command.substring(4);

        if (ledCmd == "OFF") {
            currentLEDMode = MODE_OFF;
        } else if (ledCmd == "RAINBOW") {
            currentLEDMode = MODE_RAINBOW;
        } else if (ledCmd == "PULSE") {
            currentLEDMode = MODE_PULSE;
        } else if (ledCmd == "GSR") {
            currentLEDMode = MODE_GSR_VISUALIZATION;
        } else if (ledCmd.startsWith("COLOR:")) {
            currentLEDMode = MODE_SOLID_COLOR;
            parseColorCommand(ledCmd.substring(6));
        }

    } else if (command.startsWith("BRIGHTNESS:")) {
        int brightness = command.substring(11).toInt();
        brightness = constrain(brightness, 0, 255);
        strip.setBrightness(brightness);

    } else if (command.startsWith("GROUP:")) {
        int newGroup = command.substring(6).toInt();
        groupNumber = constrain(newGroup, 1, 5);
        sendStatus("GROUP_CHANGED_TO_" + String(groupNumber));

    } else if (command == "PING") {
        sendStatus("PONG");

    } else if (command == "sim") {
        simulationMode = !simulationMode;
        if (simulationMode) {
            currentLEDMode = MODE_GSR_DOWNSTREAM;
            sendStatus("SIMULATION_ON");
        } else {
            currentLEDMode = MODE_GSR_VISUALIZATION;
            sendStatus("SIMULATION_OFF");
        }

    } else if (command.startsWith("{\"ema\":")) {
        int startIdx = command.indexOf(':') + 1;
        int endIdx = command.indexOf('}');
        if (startIdx > 0 && endIdx > startIdx) {
            String emaStr = command.substring(startIdx, endIdx);
            simulatedEma = emaStr.toFloat();

            if (simulationMode) {
                currentLEDMode = MODE_GSR_DOWNSTREAM;
            }
        }
    }
}

void GSRVisualizer::parseColorCommand(String colorStr) {
    int commaIndex1 = colorStr.indexOf(',');
    int commaIndex2 = colorStr.lastIndexOf(',');

    if (commaIndex1 > 0 && commaIndex2 > commaIndex1) {
        int r = colorStr.substring(0, commaIndex1).toInt();
        int g = colorStr.substring(commaIndex1 + 1, commaIndex2).toInt();
        int b = colorStr.substring(commaIndex2 + 1).toInt();

        solidColor = strip.Color(r, g, b);
    }
}

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         CONFIGURATION METHODS
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void GSRVisualizer::setSpikeThreshold(float threshold) {
    spikeThreshold = threshold;
}

void GSRVisualizer::setExponentialAlpha(float newAlpha) {
    alpha = constrain(newAlpha, 0.0, 1.0);
}

//──────── Hard Mode Configuration Methods ────────
void GSRVisualizer::setLEDMode(LEDMode mode) {
    currentLEDMode = mode;
}

void GSRVisualizer::setGroupNumber(int group) {
    groupNumber = constrain(group, 1, 5);
}

void GSRVisualizer::setSimulationMode(bool enabled) {
    simulationMode = enabled;
}

void GSRVisualizer::setSimulatedEma(float value) {
    simulatedEma = value;
}

LEDMode GSRVisualizer::getLEDMode() {
    return currentLEDMode;
}

bool GSRVisualizer::isSimulationMode() {
    return simulationMode;
}

/*
╚══════════════════════════════════════════════════════════════════════════╝
                      END OF HARD MODE IMPLEMENTATIONS
╚══════════════════════════════════════════════════════════════════════════╝
*/