/*
╔══════════════════════════════════════════════════════════════════════════╗
║                         GSR VISUALIZER LIBRARY                            ║
║            Signal Processing, LED Effects & Web Serial                    ║
╚══════════════════════════════════════════════════════════════════════════╝
*/

#ifndef GSR_VISUALIZER_H
#define GSR_VISUALIZER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                           LED MODES
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

enum LEDMode {
  MODE_GSR_VISUALIZATION,   // Default GSR visualization
  MODE_GSR_DOWNSTREAM,      // Downstream GSR visualization
  MODE_SOLID_COLOR,          // Solid color
  MODE_PULSE,                // Pulsing pattern
  MODE_RAINBOW,              // Rainbow effect
  MODE_OFF                   // All LEDs off
};

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                         GROUP COLORS
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct GroupColor {
  int r, g, b;
};

//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//                        MAIN CLASS
//━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

class GSRVisualizer {
  private:
    Adafruit_NeoPixel& strip;
    int numLeds;

    //──────── Signal Processing Variables ────────
    int* readings;
    int numReadings;
    int readIndex;
    int total;
    float alpha;  // Exponential filter smoothing factor

    //──────── Spike Detection Variables ────────
    bool inSpike;
    float spikeThreshold;
    float lastFilteredValue;

    //──────── Web Serial Animation Variables ────────
    float animationPosition;
    unsigned long lastAnimationTime;
    int trailLength;
    LEDMode currentLEDMode;
    uint32_t solidColor;
    int groupNumber;
    GroupColor* groupColors;

    //──────── Advanced Processing Variables ────────
    float adaptiveBaseline;
    float adaptiveAlpha;
    float normalizedEma;
    float shortTermBaseline;

    //──────── Simulation Variables ────────
    bool simulationMode;
    float simulatedEma;

  public:
    GSRVisualizer(Adafruit_NeoPixel& ledStrip, int numSamples = 10);
    ~GSRVisualizer();

    //━━━━━━━━━ Signal Processing Methods ━━━━━━━━━
    int calculateMovingAverage(int newReading);
    float applyExponentialFilter(int newReading, float currentFiltered);
    void checkForSpikes(float filteredValue);

    //━━━━━━━━━ Advanced Processing Methods ━━━━━━━━━
    void updateAdaptiveBaseline(float emaValue);
    float calculateNormalizedEma(float emaValue, int gsrMin, int gsrMax);
    float getCombinedSignal(float emaValue, float emaDerivative, int gsrMin, int gsrMax);

    //━━━━━━━━━ LED Visualization Methods ━━━━━━━━━
    void updateLEDDisplay(float value, int gsrMin, int gsrMax);
    uint32_t getColorForLevel(int ledIndex);
    void updateLEDs(float emaValue, int gsrMin, int gsrMax, float emaDerivative);

    //━━━━━━━━━ LED Animation Methods ━━━━━━━━━
    void showCalibrationAnimation();
    void flashSuccess();
    void setAllPixels(int r, int g, int b);
    void addBreathingEffect(int ledIndex);

    /*
    ╔════════════════════════════════════════════════════════════════╗
    ║                    HARD MODE SPECIFIC METHODS                   ║
    ╠════════════════════════════════════════════════════════════════╣
    ║  The following methods are specifically for Hard Mode with      ║
    ║  Web Serial communication and advanced LED animations           ║
    ╚════════════════════════════════════════════════════════════════╝
    */

    //━━━━━━━━━ Hard Mode: Advanced LED Animations ━━━━━━━━━
    void visualizeGSR(bool downstream, float emaValue, int gsrMin, int gsrMax, float emaDerivative);
    void setSolidColor();
    void showPulse();
    void showRainbow();

    //━━━━━━━━━ Hard Mode: Web Serial Communication ━━━━━━━━━
    void sendDataToP5(float emaValue);
    void sendStatus(String status);
    void processCommand(String command, float& emaValue, float& baseline);
    void parseColorCommand(String colorStr);

    //━━━━━━━━━ Configuration Methods ━━━━━━━━━
    void setSpikeThreshold(float threshold);
    void setExponentialAlpha(float newAlpha);
    void setLEDMode(LEDMode mode);
    void setGroupNumber(int group);
    void setSimulationMode(bool enabled);
    void setSimulatedEma(float value);
    LEDMode getLEDMode();
    bool isSimulationMode();
};

#endif