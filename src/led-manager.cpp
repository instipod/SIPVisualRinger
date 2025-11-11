#include <led-manager.h>

LedManager::LedManager() {
    strip = Adafruit_NeoPixel(WS2811_COUNT, WS2811_PIN, NEO_GRB + NEO_KHZ800);
}

void LedManager::setPattern(int pattern) {
    if (pattern == runningPattern) { return; }
    lastTick = 0;
    ledStage = 0;
    runningPattern = pattern;
    Serial.println("LED pattern changed to " + String(pattern));
}

void LedManager::init() {
    strip.begin();
    strip.setBrightness(250);
    for (int i = 0; i < WS2811_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
    }
    strip.show();
}

void LedManager::handle() {
    switch (runningPattern) {
        case PATTERN1:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
                }
                lastTick = millis();
            }
            break;
        case PATTERN2:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
                }
                lastTick = millis();
            }
            break;
        case PATTERN3:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green
                }
                lastTick = millis();
            }
            break;
        case PATTERN4:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, 255)); // Blue
                }
                lastTick = millis();
            }
            break;
        case PATTERN5:
            if (millis() - lastTick > 250) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color((255 * ledStage), 0, 0)); // Red or Off
                }
                if (ledStage == 0) {
                    ledStage = 1;
                } else {
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case PATTERN6:
            if (millis() - lastTick > 250) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, (255 * ledStage), 0)); // Green or Off
                }
                if (ledStage == 0) {
                    ledStage = 1;
                } else {
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case PATTERN7:
            if (millis() - lastTick > 250) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, (255 * ledStage))); // Blue or Off
                }
                if (ledStage == 0) {
                    ledStage = 1;
                } else {
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case PATTERN8:
            if (millis() - lastTick > 250) {
                if (ledStage == 0) {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
                    }
                    ledStage = 1;
                } else {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green
                    }
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case PATTERN9:
            if (millis() - lastTick > 250) {
                if (ledStage == 0) {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
                    }
                    ledStage = 1;
                } else {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 0, 255)); // Green
                    }
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case PATTERN10:
            if (millis() - lastTick > 250) {
                if (ledStage == 0) {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 255, 0)); // Red
                    }
                    ledStage = 1;
                } else {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 0, 255)); // Green
                    }
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        default:
            if (millis() - lastTick > 1000) {
                Serial.println("Bad LED pattern in memory!");
            }
            lastTick = millis();
            break;
    }

    strip.show();
}