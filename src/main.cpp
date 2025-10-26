#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <Adafruit_NeoPixel.h>
#include <sip.h>
#include <runtime.h>
#include <leds.h>
#include <configserver.h>

// Build configuration
#define WS2811_PIN 12
#define WS2811_COUNT 3
#define ETH_PHY_ADDR 1
#define ETH_PHY_MDC 23
#define ETH_PHY_MDIO 18
#define ETH_PHY_POWER 16
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
#define ETH_PHY_TYPE ETH_PHY_LAN8720

// Global variables
Adafruit_NeoPixel strip(WS2811_COUNT, WS2811_PIN, NEO_GRB + NEO_KHZ800);
Runtime runtime;
ConfigServer configServer = ConfigServer(runtime);

bool ledState = false;
unsigned long lastLedToggle = 0;
unsigned long ledFlashInterval = LED_FAST_FLASH;


void onIPAddressAssigned() {
  runtime.currentLedMode = LED_IDLE;

  runtime.ip_begin();
}

void onIPAddressLost() {
  runtime.currentLedMode = LED_CONNECTING;

  runtime.ip_end();
}

void WiFiEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_ETH_GOT_IP) {
      Serial.print("ETH Got IP: ");
      Serial.println(ETH.localIP());
      Serial.print("Gateway: ");
      Serial.println(ETH.gatewayIP());

      runtime.ethernetIP = ETH.localIP().toString();
      runtime.ethernetMAC = ETH.macAddress();

      onIPAddressAssigned();
  } else if (event == ARDUINO_EVENT_ETH_LOST_IP || event == ARDUINO_EVENT_ETH_DISCONNECTED || event == ARDUINO_EVENT_ETH_STOP) {
      Serial.println("ETH Lost IP");

      runtime.ethernetIP = "0.0.0.0";

      onIPAddressLost();
  }
}

void updateLEDs() {
  // Determine LED mode based on system state
  LedMode targetMode;

  // Priority:  BOOTUP, CONNECTING, INCOMING_CALL, SIP_ERROR, IDLE
  if (runtime.currentLedMode == LED_BOOTUP) {
    targetMode = LED_BOOTUP;
    ledFlashInterval = LED_SLOW_FLASH;
  } else if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
    targetMode = LED_CONNECTING;
    ledFlashInterval = LED_SLOW_FLASH;
  } else if (runtime.sipLine1.is_ringing() && runtime.currentLedMode == LED_INCOMING_CALL) {
    targetMode = LED_INCOMING_CALL;
    ledFlashInterval = LED_FAST_FLASH;
  } else if (!runtime.sipLine1.is_registered()) {
    targetMode = LED_SIP_ERROR;
    ledFlashInterval = LED_SLOW_FLASH;
  } else {
    targetMode = LED_IDLE;
    ledFlashInterval = LED_FAST_FLASH;
  }
  
  // Update mode if changed
  if (runtime.currentLedMode != targetMode) {
    runtime.currentLedMode = targetMode;
    lastLedToggle = millis();
    ledState = false;
  }
  
  // Handle LED patterns based on mode
  switch (runtime.currentLedMode) {
    case LED_BOOTUP:
      // Solid red on all LEDs
      strip.setPixelColor(0, strip.Color(255, 0, 0)); // Red
      strip.setPixelColor(1, strip.Color(255, 0, 0)); // Red
      strip.setPixelColor(2, strip.Color(255, 0, 0)); // Red
      strip.show();
      break;

    case LED_CONNECTING:
      // Slow blue blink on all 3 LEDs
      if (millis() - lastLedToggle > ledFlashInterval) {
        ledState = !ledState;
        if (ledState) {
          // Blue
          strip.setPixelColor(0, strip.Color(0, 0, 255));
          strip.setPixelColor(1, strip.Color(0, 0, 255));
          strip.setPixelColor(2, strip.Color(0, 0, 255));
        } else {
          // Off
          strip.setPixelColor(0, strip.Color(0, 0, 0));
          strip.setPixelColor(1, strip.Color(0, 0, 0));
          strip.setPixelColor(2, strip.Color(0, 0, 0));
        }
        strip.show();
        lastLedToggle = millis();
      }
      break;

    case LED_IDLE:
      // Solid green on first LED only
      strip.setPixelColor(0, strip.Color(0, 255, 0)); // Green
      strip.setPixelColor(1, strip.Color(0, 0, 0));   // Off
      strip.setPixelColor(2, strip.Color(0, 0, 0));   // Off
      strip.show();
      break;
      
    case LED_INCOMING_CALL:
      // Fast alternating red/orange flash on all 3 LEDs
      if (millis() - lastLedToggle > ledFlashInterval) {
        ledState = !ledState;
        if (ledState) {
          // Red
          strip.setPixelColor(0, strip.Color(255, 0, 0));
          strip.setPixelColor(1, strip.Color(255, 0, 0));
          strip.setPixelColor(2, strip.Color(255, 0, 0));
        } else {
          // Orange
          strip.setPixelColor(0, strip.Color(255, 165, 0));
          strip.setPixelColor(1, strip.Color(255, 165, 0));
          strip.setPixelColor(2, strip.Color(255, 165, 0));
        }
        strip.show();
        lastLedToggle = millis();
      }
      break;
      
    case LED_SIP_ERROR:
      // Slow red blink on all 3 LEDs
      if (millis() - lastLedToggle > ledFlashInterval) {
        ledState = !ledState;
        if (ledState) {
          // Red
          strip.setPixelColor(0, strip.Color(255, 0, 0));
          strip.setPixelColor(1, strip.Color(255, 0, 0));
          strip.setPixelColor(2, strip.Color(255, 0, 0));
        } else {
          // Off
          strip.setPixelColor(0, strip.Color(0, 0, 0));
          strip.setPixelColor(1, strip.Color(0, 0, 0));
          strip.setPixelColor(2, strip.Color(0, 0, 0));
        }
        strip.show();
        lastLedToggle = millis();
      }
      break;
  }
}

void initEthernet() {
  Serial.println("Initializing Ethernet...");
  
  WiFi.onEvent(WiFiEvent);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
  ETH.setHostname(runtime.deviceHostname.c_str());

  runtime.currentLedMode = LED_CONNECTING;
  updateLEDs();
  
  // Wait for connection
  int timeout = 0;
  while (!ETH.linkUp() && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (ETH.linkUp()) {
    Serial.println("\nEthernet connected!");
    runtime.ethernetMAC = ETH.macAddress();
  } else {
    Serial.println("\nEthernet connection failed!");
    delay(60000);
    ESP.restart();
  }
}

void initMDNS() {
  if (runtime.mDNSEnabled) {
    MDNS.begin(runtime.deviceHostname.c_str());
    MDNS.addService("http", "_tcp", 80);
    MDNS.addService("visualalert", "_tcp", 80);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("SIP Alerter is starting...");

  runtime.init();
  runtime.ethernetMAC = ETH.macAddress();
  runtime.deviceHostname = "VisualAlert-" + runtime.ethernetMAC.substring(runtime.ethernetMAC.length() - 5, runtime.ethernetMAC.length() - 1);

  strip.begin();
  strip.setBrightness(25);
  updateLEDs();

  runtime.load_configuration();

  initEthernet();

  initMDNS();

  configServer.init();

  // Wait a bit for Ethernet to fully initialize
  delay(500);

  runtime.lldp.init(runtime.deviceHostname, "ESP32 SIP");
  
  Serial.println("Setup complete!");
}

void loop() {
  // Handle web server
  configServer.handle();
  
  // Handle SIP messages
  runtime.handle();
  
  // Update LED status
  updateLEDs();
}
