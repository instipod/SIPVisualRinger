#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <Preferences.h>
#include <sip.h>
#include <runtime.h>
#include <configserver.h>
#include <SPI.h>

// Ethernet configuration
#define ETH_SPI_SCK 13
#define ETH_SPI_MISO 12
#define ETH_SPI_MOSI 11
#define ETH_PHY_CS 14
#define ETH_PHY_IRQ 10
#define ETH_PHY_RST 9
#define ETH_PHY_ADDR 1
#define ETH_PHY_TYPE ETH_PHY_W5500

// Global variables
Runtime runtime;
ConfigServer configServer = ConfigServer(runtime);

bool ledState = false;
unsigned long lastLedToggle = 0;
unsigned long ledFlashInterval = LED_FAST_FLASH;


void onIPAddressAssigned() {
  configServer.init();
  runtime.ip_begin();
}

void onIPAddressLost() {
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

int getLEDPattern() {
  if (runtime.sipLine2.is_registered() && runtime.sipLine2.is_ringing()) {
    return runtime.line2RingPattern;
  }
  if (runtime.sipLine1.is_registered() && runtime.sipLine1.is_ringing()) {
    return runtime.line1RingPattern;
  }
  if (ETH.connected()) {
    return runtime.idlePattern;
  }
  return 0;
}

int getRelayPattern(int config) {
  switch(config) {
    case RELAY_DISABLED:
      return RELAY_OFF;
      break;
    case ON_WHILE_RINGING:
      if (runtime.sipLine2.is_registered() && runtime.sipLine2.is_ringing()) {
        return RELAY_ON;
      }
      if (runtime.sipLine1.is_registered() && runtime.sipLine1.is_ringing()) {
        return RELAY_ON;
      }
      return RELAY_OFF;
      break;
    case ON_WHILE_LINE1:
      if (runtime.sipLine1.is_registered() && runtime.sipLine1.is_ringing()) {
        return RELAY_ON;
      }
      return RELAY_OFF;
      break;
    case ON_WHILE_LINE2:
      if (runtime.sipLine2.is_registered() && runtime.sipLine2.is_ringing()) {
        return RELAY_ON;
      }
      return RELAY_ON;
      break;
    case ON_WHILE_ERROR:
      if (runtime.sipLine1.is_configured() && !runtime.sipLine1.is_registered()) {
        return RELAY_ON;
      }
      if (runtime.sipLine2.is_configured() && !runtime.sipLine2.is_registered()) {
        return RELAY_ON;
      }
      return RELAY_OFF;
      break;
    case TOGGLE_WHILE_RINGING:
      if (runtime.sipLine2.is_registered() && runtime.sipLine2.is_ringing()) {
        return TOGGLE;
      }
      if (runtime.sipLine1.is_registered() && runtime.sipLine1.is_ringing()) {
        return TOGGLE;
      }
      return RELAY_OFF;
      break;
    case TOGGLE_WHILE_LINE1:
      if (runtime.sipLine1.is_registered() && runtime.sipLine1.is_ringing()) {
        return TOGGLE;
      }
      return RELAY_OFF;
      break;
    case TOGGLE_WHILE_LINE2:
      if (runtime.sipLine2.is_registered() && runtime.sipLine2.is_ringing()) {
        return TOGGLE;
      }
      return RELAY_OFF;
      break;
    case TOGGLE_WHILE_ERROR:
      if (runtime.sipLine1.is_configured() && !runtime.sipLine1.is_registered()) {
        return TOGGLE;
      }
      if (runtime.sipLine2.is_configured() && !runtime.sipLine2.is_registered()) {
        return TOGGLE;
      }
      return RELAY_OFF;
      break;
    default:
      return RELAY_OFF;
      break;
  }
  return RELAY_OFF;
}

void updateLEDs() {
  runtime.ledManager.setPattern(getLEDPattern());

  runtime.relay1.setState(getRelayPattern(runtime.relay1Config));
  runtime.relay2.setState(getRelayPattern(runtime.relay2Config));

  //One time update while we are booting
  runtime.ledManager.handle();
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
    
  // Update LED status
  updateLEDs();

  // Handle SIP messages
  runtime.handle();
}
