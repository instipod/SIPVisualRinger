#include <relay-manager.h>

RelayManager::RelayManager(int relayPin) {
    this->relayPin = relayPin;
}

void RelayManager::setState(int state) {
    if (state == relayState) { return; }
    lastTick = 0;
    relayStage = 0;
    relayState = state;
    Serial.println("Relay pattern changed to " + String(relayState));
}

void RelayManager::init() {
    pinMode(this->relayPin, OUTPUT);
}

void RelayManager::handle() {
    switch (relayState) {
        case RELAY_OFF:
            if (millis() - lastTick > 1000) {
                digitalWrite(this->relayPin, LOW);
                lastTick = millis();
            }
            break;
        case RELAY_ON:
            if (millis() - lastTick > 1000) {
                digitalWrite(this->relayPin, HIGH);
                lastTick = millis();
            }
            break;
        case TOGGLE:
            if (millis() - lastTick > 1000) {
                if (relayStage == 0) {
                    digitalWrite(this->relayPin, HIGH);
                    relayStage = 1;
                } else {
                    digitalWrite(this->relayPin, LOW);
                    relayStage = 0;
                }
                lastTick = millis();
            }
            break;
        default:
            if (millis() - lastTick > 1000) {
                Serial.println("Bad relay pattern in memory!");
            }
            lastTick = millis();
            break;
    }
}