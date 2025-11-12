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
        case LED_OFF:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
                }
                lastTick = millis();
            }
            break;
        case RED_SOLID:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
                }
                lastTick = millis();
            }
            break;
        case GREEN_SOLID:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green
                }
                lastTick = millis();
            }
            break;
        case BLUE_SOLID:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, 255)); // Blue
                }
                lastTick = millis();
            }
            break;
        case YELLOW_SOLID:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(255, 255, 0)); // Yellow
                }
                lastTick = millis();
            }
            break;
        case PURPLE_SOLID:
            if (millis() - lastTick > 1000) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 255, 255)); // Purple
                }
                lastTick = millis();
            }
            break;
        case RED_FLASH:
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
        case GREEN_FLASH:
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
        case BLUE_FLASH:
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
        case YELLOW_FLASH:
            if (millis() - lastTick > 250) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color((255 * ledStage), (255 * ledStage), 0)); // Yellow or Off
                }
                if (ledStage == 0) {
                    ledStage = 1;
                } else {
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case PURPLE_FLASH:
            if (millis() - lastTick > 250) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, (255 * ledStage), (255 * ledStage))); // Purple or Off
                }
                if (ledStage == 0) {
                    ledStage = 1;
                } else {
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case RED_GREEN_FLASH:
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
        case RED_BLUE_FLASH:
            if (millis() - lastTick > 250) {
                if (ledStage == 0) {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
                    }
                    ledStage = 1;
                } else {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 0, 255)); // Blue
                    }
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case RED_YELLOW_FLASH:
            if (millis() - lastTick > 250) {
                if (ledStage == 0) {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
                    }
                    ledStage = 1;
                } else {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(255, 255, 0)); // Yellow
                    }
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case RED_PURPLE_FLASH:
            if (millis() - lastTick > 250) {
                if (ledStage == 0) {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
                    }
                    ledStage = 1;
                } else {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 255, 255)); // Purple
                    }
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case GREEN_BLUE_FLASH:
            if (millis() - lastTick > 250) {
                if (ledStage == 0) {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green
                    }
                    ledStage = 1;
                } else {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 0, 255)); // Blue
                    }
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case GREEN_YELLOW_FLASH:
            if (millis() - lastTick > 250) {
                if (ledStage == 0) {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green
                    }
                    ledStage = 1;
                } else {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(255, 255, 0)); // Yellow
                    }
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case GREEN_PURPLE_FLASH:
            if (millis() - lastTick > 250) {
                if (ledStage == 0) {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green
                    }
                    ledStage = 1;
                } else {
                    for (int i = 0; i < WS2811_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 255, 255)); // Purple
                    }
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case RED_CHASE:
            if (millis() - lastTick > 100) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
                }
                strip.setPixelColor(ledStage, strip.Color(255, 0, 0));
                if (ledStage != WS2811_COUNT) {
                    strip.setPixelColor(ledStage + 1, strip.Color(255, 0, 0));
                } else {
                    strip.setPixelColor(0, strip.Color(255, 0, 0));
                }
                ledStage += 1;
                if (ledStage > WS2811_COUNT) {
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case GREEN_CHASE:
            if (millis() - lastTick > 100) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
                }
                strip.setPixelColor(ledStage, strip.Color(255, 0, 0));
                if (ledStage != WS2811_COUNT) {
                    strip.setPixelColor(ledStage + 1, strip.Color(0, 255, 0));
                } else {
                    strip.setPixelColor(0, strip.Color(0, 255, 0));
                }
                ledStage += 1;
                if (ledStage > WS2811_COUNT) {
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case BLUE_CHASE:
            if (millis() - lastTick > 100) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
                }
                strip.setPixelColor(ledStage, strip.Color(0, 0, 255));
                if (ledStage != WS2811_COUNT) {
                    strip.setPixelColor(ledStage + 1, strip.Color(0, 0, 255));
                } else {
                    strip.setPixelColor(0, strip.Color(0, 0, 255));
                }
                ledStage += 1;
                if (ledStage > WS2811_COUNT) {
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case YELLOW_CHASE:
            if (millis() - lastTick > 100) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
                }
                strip.setPixelColor(ledStage, strip.Color(255, 255, 0));
                if (ledStage != WS2811_COUNT) {
                    strip.setPixelColor(ledStage + 1, strip.Color(255, 255, 0));
                } else {
                    strip.setPixelColor(0, strip.Color(255, 255, 0));
                }
                ledStage += 1;
                if (ledStage > WS2811_COUNT) {
                    ledStage = 0;
                }
                lastTick = millis();
            }
            break;
        case PURPLE_CHASE:
            if (millis() - lastTick > 100) {
                for (int i = 0; i < WS2811_COUNT; i++) {
                    strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
                }
                strip.setPixelColor(ledStage, strip.Color(0, 255, 255));
                if (ledStage != WS2811_COUNT) {
                    strip.setPixelColor(ledStage + 1, strip.Color(0, 255, 255));
                } else {
                    strip.setPixelColor(0, strip.Color(0, 255, 255));
                }
                ledStage += 1;
                if (ledStage > WS2811_COUNT) {
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