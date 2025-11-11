#ifndef LEDMANAGER_H
#define LEDMANAGER_H
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_FAST_FLASH 250
#define LED_SLOW_FLASH 2000

#define WS2811_PIN 2
#define WS2811_COUNT 10

enum LedPattern {
  OFF, //Dark
  RED_SOLID, //Red Solid
  GREEN_SOLID, //Green Solid
  BLUE_SOLID, //Blue Solid
  YELLOW_SOLID, //Yellow Solid
  PURPLE_SOLID, //Purple Solid
  RED_FLASH, //Red Flash
  GREEN_FLASH, //Green Flash
  BLUE_FLASH, //Blue Flash
  YELLOW_FLASH, //Yellow Flash
  PURPLE_FLASH, //Purple Flash
  RED_GREEN_FLASH, //Red/Green Flash
  RED_BLUE_FLASH, //Red/Blue Flash
  RED_YELLOW_FLASH, //Red/Yellow Flash
  RED_PURPLE_FLASH, //Red/Purple Flash
  GREEN_BLUE_FLASH, //Green/Blue Flash
  GREEN_YELLOW_FLASH, //Green/Yellow Flash
  GREEN_PURPLE_FLASH, //Green/Purple Flash
  RED_CHASE, //Red Chase
  GREEN_CHASE, //Green Chase
  BLUE_CHASE, //Blue Chase
  YELLOW_CHASE, //Yellow Chase
  PURPLE_CHASE //Purple Chase
};

class LedManager {
  private:
    Adafruit_NeoPixel strip; //(WS2811_COUNT, WS2811_PIN, NEO_GRB + NEO_KHZ800);
    long lastTick = 0;
    int ledStage = 0;
  public:
    int runningPattern = OFF;

    LedManager();
    void setPattern(int pattern);
    void init();
    void handle();
};
#endif