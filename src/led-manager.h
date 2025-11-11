#ifndef LEDMANAGER_H
#define LEDMANAGER_H
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_FAST_FLASH 250
#define LED_SLOW_FLASH 2000

#define WS2811_PIN 2
#define WS2811_COUNT 10

enum LedPattern {
  PATTERN1, //Dark
  PATTERN2, //Red Solid
  PATTERN3, //Green Solid
  PATTERN4, //Blue Solid
  PATTERN5, //Red Flash
  PATTERN6, //Green Flash
  PATTERN7, // Blue Flash
  PATTERN8, //Red/Green Flash
  PATTERN9, //Red/Blue Flash
  PATTERN10 //Green/Blue Flash
};

class LedManager {
    private:
        Adafruit_NeoPixel strip; //(WS2811_COUNT, WS2811_PIN, NEO_GRB + NEO_KHZ800);
        int runningPattern = PATTERN1;
        long lastTick = 0;
        int ledStage = 0;
    public:
        LedManager();
        void setPattern(int pattern);
        void init();
        void handle();
};
#endif