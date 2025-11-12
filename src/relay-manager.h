#ifndef RELAYMANAGER_H
#define RELAYMANAGER_H
#include <Arduino.h>

enum RelayConfiguration {
  RELAY_DISABLED,
  ON_WHILE_RINGING,
  ON_WHILE_LINE1,
  ON_WHILE_LINE2,
  ON_WHILE_ERROR,
  TOGGLE_WHILE_RINGING,
  TOGGLE_WHILE_LINE1,
  TOGGLE_WHILE_LINE2,
  TOGGLE_WHILE_ERROR
};

enum RelayPattern {
    RELAY_OFF,
    RELAY_ON,
    TOGGLE
};

class RelayManager {
  private:
    int relayPin = 0;
    
    long lastTick = 0;
    int relayStage = 0;
  public:
    int relayState = RELAY_DISABLED;

    RelayManager(int relayPin);
    void setState(int state);
    void init();
    void handle();
};
#endif