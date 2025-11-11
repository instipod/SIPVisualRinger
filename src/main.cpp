#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <sip.h>
#include <runtime.h>
#include <leds.h>
#include <configserver.h>
#include <SPI.h>

// Build configuration
#define WS2811_PIN 2
#define WS2811_COUNT 10

// Ethernet configuration
#define ETH_SPI_SCK 13
#define ETH_SPI_MISO 12
#define ETH_SPI_MOSI 11
#define ETH_PHY_CS 14
#define ETH_PHY_IRQ 10
#define ETH_PHY_RST 9
#define ETH_PHY_ADDR 1
#define ETH_PHY_TYPE ETH_PHY_W5500

// Relay configuration
#define RELAY1 6
#define RELAY2 5

// Global variables
Adafruit_NeoPixel strip(WS2811_COUNT, WS2811_PIN, NEO_GRB + NEO_KHZ800);
Runtime runtime;
ConfigServer configServer = ConfigServer(runtime);

bool ledState = false;
unsigned long lastLedToggle = 0;
unsigned long ledFlashInterval = LED_FAST_FLASH;


void onIPAddressAssigned() {
  runtime.currentLedMode = LED_IDLE;

  configServer.init();

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
  } else if (runtime.sipLine1.is_registered() && runtime.sipLine1.is_ringing()) {
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
      for (int i = 0; i < WS2811_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
      }
      strip.show();
      break;

    case LED_CONNECTING:
      // Slow blue blink on all 3 LEDs
      if (millis() - lastLedToggle > ledFlashInterval) {
        ledState = !ledState;
        if (ledState) {
          // Blue
          for (int i = 0; i < WS2811_COUNT; i++) {
            strip.setPixelColor(i, strip.Color(0, 0, 255)); // Blue
          }
        } else {
          // Off
          for (int i = 0; i < WS2811_COUNT; i++) {
            strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
          }
        }
        strip.show();
        lastLedToggle = millis();
      }
      break;

    case LED_IDLE:
      // Solid green on alternating LEDs only
      for (int i = 0; i < WS2811_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
      }
      strip.setPixelColor(0, strip.Color(0, 128, 0)); // Green
      strip.setPixelColor(2, strip.Color(0, 128, 0)); // Green
      strip.setPixelColor(4, strip.Color(0, 128, 0)); // Green
      strip.setPixelColor(6, strip.Color(0, 128, 0)); // Green
      strip.setPixelColor(8, strip.Color(0, 128, 0)); // Green
      strip.show();
      digitalWrite(RELAY1, LOW);
      break;
      
    case LED_INCOMING_CALL:
      // Fast alternating red/orange flash on all 3 LEDs
      if (millis() - lastLedToggle > ledFlashInterval) {
        ledState = !ledState;
        if (ledState) {
          // Red
          for (int i = 0; i < WS2811_COUNT; i++) {
            strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
          }
          digitalWrite(RELAY1, HIGH);
        } else {
          // Orange
          for (int i = 0; i < WS2811_COUNT; i++) {
            strip.setPixelColor(i, strip.Color(255, 165, 0)); // Orange
          }
          digitalWrite(RELAY1, LOW);
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
          for (int i = 0; i < WS2811_COUNT; i++) {
            strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red
          }
        } else {
          // Off
          for (int i = 0; i < WS2811_COUNT; i++) {
            strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
          }
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

  uint8_t ethMac[6];
  runtime.get_ethernet_mac(ethMac);
  esp_iface_mac_addr_set(ethMac, ESP_MAC_BASE);

  Serial.println("Initializing SPI...");
  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  Serial.println("Initializing W5500 PHY...");
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI);

  ETH.setHostname(runtime.deviceHostname.c_str());

  Serial.println("Ethernet is initialized with MAC " + ETH.macAddress());

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
  } else {
    Serial.println("\nEthernet connection failed!");
    delay(60000);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("SIP Alerter is starting...");

  runtime.init();

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  strip.begin();
  strip.setBrightness(250);
  updateLEDs();

  runtime.load_configuration();

  Serial.println("System MAC Address: " + runtime.get_ethernet_mac_address());
  Serial.println("System Hostname: " + runtime.deviceHostname);

  initEthernet();

  // Wait a bit for Ethernet to fully initialize
  delay(500);

  // Note: configServer.init() is now called in onIPAddressAssigned()
  // after the ESP32 receives an IP address from DHCP

  runtime.lldp.init();

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
