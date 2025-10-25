#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <MD5Builder.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>
// ESP-IDF includes for raw Ethernet frames
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"

// Build configuration
#define WS2811_PIN 12
#define WS2811_COUNT 3
#define ETH_PHY_ADDR 1
#define ETH_PHY_MDC 23
#define ETH_PHY_MDIO 18
#define ETH_PHY_POWER 16
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
#define ETH_PHY_TYPE ETH_PHY_LAN8720
#define SIP_REGISTER_INTERVAL 3600000 // 1 hour
#define SIP_USER_AGENT "ESP32-SIP/1.1"
#define LLDP_INTERVAL 30000 // 30 seconds
#define LED_FAST_FLASH 250
#define LED_SLOW_FLASH 2000

// LED states
enum LedMode {
  LED_BOOTUP,         // Solid red (all LEDs)
  LED_CONNECTING,     // Slow blink blue (all LEDs)
  LED_IDLE,           // Solid green (1 LED)
  LED_INCOMING_CALL,  // Fast flash red/orange (all LEDs)
  LED_SIP_ERROR       // Slow blink red (all LEDs)
};

// Global variables
Adafruit_NeoPixel strip(WS2811_COUNT, WS2811_PIN, NEO_GRB + NEO_KHZ800);
Preferences preferences;
String deviceHostname = "esp32-sip";
String ethIPAddress = "";
String ethMACAddress = "";
esp_eth_handle_t eth_handle = NULL;
WebServer server(80);

// SIP variables
String sipServer = "";
int sipPort = 5060;
String sipUsername = "";
String sipPassword = "";
String sipRealm = "";
bool sipRegistered = false;
unsigned long lastRegisterTime = 0;
const unsigned long registerInterval = SIP_REGISTER_INTERVAL;
String currentCallID = "";
String currentFromTag = "";
String currentToTag = "";
int authAttempts = 0;
unsigned long lastAuthAttempt = 0;
WiFiUDP udpSIP;

// MDNS variables
bool mdnsEnabled = true;

// LLDP variables
bool lldpEnabled = true;
unsigned long lastLLDPTime = 0;
const unsigned long lldpInterval = LLDP_INTERVAL;

// LED variables
LedMode currentLedMode = LED_BOOTUP;
bool ledState = false;
unsigned long lastLedToggle = 0;
unsigned long ledFlashInterval = LED_FAST_FLASH;

// Function declarations
esp_eth_handle_t getEthHandle() {
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
  if (netif == NULL) {
    Serial.println("Could not get netif handle");
    return NULL;
  }
  
  // Get the IO driver (returns void* which is the esp_eth_handle_t)
  void *driver = esp_netif_get_io_driver(netif);
  if (driver == NULL) {
    Serial.println("Could not get IO driver");
    return NULL;
  }
  
  // The driver is the esp_eth_handle_t
  return (esp_eth_handle_t)driver;
}

void onIPAddressAssigned() {
  currentLedMode = LED_IDLE;
}

void onIPAddressLost() {
  currentLedMode = LED_CONNECTING;

  // Clear call state
  sipRegistered = false;
  currentCallID = "";
  currentFromTag = "";
  currentToTag = "";
}

void WiFiEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_ETH_GOT_IP) {
      Serial.print("ETH Got IP: ");
      Serial.println(ETH.localIP());
      Serial.print("Gateway: ");
      Serial.println(ETH.gatewayIP());
      onIPAddressAssigned();
  } else if (event == ARDUINO_EVENT_ETH_LOST_IP || event == ARDUINO_EVENT_ETH_DISCONNECTED || event == ARDUINO_EVENT_ETH_STOP) {
      Serial.println("ETH Lost IP");
      onIPAddressLost();
  }
}

void updateLEDs() {
  // Determine LED mode based on system state
  LedMode targetMode;

  // Priority:  BOOTUP, CONNECTING, INCOMING_CALL, SIP_ERROR, IDLE
  if (currentLedMode == LED_BOOTUP) {
    targetMode = LED_BOOTUP;
    ledFlashInterval = LED_SLOW_FLASH;
  } else if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
    targetMode = LED_CONNECTING;
    ledFlashInterval = LED_SLOW_FLASH;
  } else if (currentCallID != "" && currentLedMode == LED_INCOMING_CALL) {
    targetMode = LED_INCOMING_CALL;
    ledFlashInterval = LED_FAST_FLASH;
  } else if (!sipRegistered && !sipServer.isEmpty()) {
    targetMode = LED_SIP_ERROR;
    ledFlashInterval = LED_SLOW_FLASH;
  } else {
    targetMode = LED_IDLE;
    ledFlashInterval = LED_FAST_FLASH;
  }
  
  // Update mode if changed
  if (currentLedMode != targetMode) {
    currentLedMode = targetMode;
    lastLedToggle = millis();
    ledState = false;
  }
  
  // Handle LED patterns based on mode
  switch (currentLedMode) {
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
  ETH.setHostname(deviceHostname.c_str());

  currentLedMode = LED_CONNECTING;
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
    ethIPAddress = ETH.localIP().toString();
    ethMACAddress = ETH.macAddress();
    Serial.print("IP Address: ");
    Serial.println(ethIPAddress);
    Serial.print("MAC Address: ");
    Serial.println(ethMACAddress);
  } else {
    Serial.println("\nEthernet connection failed!");
    delay(60000);
    ESP.restart();
  }
}

void loadConfiguration() {
  preferences.begin("config", false);
  
  deviceHostname = preferences.getString("hostname", "esp32-sip");
  sipServer = preferences.getString("sipServer", "");
  sipPort = preferences.getInt("sipPort", 5060);
  sipUsername = preferences.getString("sipUser", "");
  sipPassword = preferences.getString("sipPass", "");
  sipRealm = preferences.getString("sipRealm", "");
  
  preferences.end();
  
  Serial.println("Configuration loaded:");
  Serial.println("Hostname: " + deviceHostname);
  Serial.println("SIP Server: " + sipServer);
  Serial.println("SIP Username: " + sipUsername);
}

void saveConfiguration() {
  preferences.begin("config", false);
  
  preferences.putString("hostname", deviceHostname);
  preferences.putString("sipServer", sipServer);
  preferences.putInt("sipPort", sipPort);
  preferences.putString("sipUser", sipUsername);
  preferences.putString("sipPass", sipPassword);
  preferences.putString("sipRealm", sipRealm);
  
  preferences.end();
  
  Serial.println("Configuration saved!");
}

// Function definition for web server
void registerSIP();

void initWebServer() {
  // Root page - configuration interface
  server.on("/", HTTP_GET, []() {
    // Get current IP address dynamically
    String currentIP = ETH.localIP().toString();
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>ESP32 SIP/LLDP Configuration</title>";
    html += "<style>body{font-family:Arial;margin:20px;} ";
    html += "input[type=text],input[type=number],input[type=password]{width:300px;padding:8px;margin:5px 0;} ";
    html += "input[type=submit]{padding:10px 20px;background:#4CAF50;color:white;border:none;cursor:pointer;} ";
    html += "label{display:inline-block;width:150px;} ";
    html += ".status{background:#f0f0f0;padding:15px;margin:20px 0;border-radius:5px;}</style>";
    html += "</head><body>";
    html += "<h1>ESP32 SIP/LLDP Device</h1>";
    
    // Status section
    html += "<div class='status'>";
    html += "<h2>Status</h2>";
    html += "<p><strong>IP Address:</strong> " + currentIP + "</p>";
    html += "<p><strong>MAC Address:</strong> " + ethMACAddress + "</p>";
    html += "<p><strong>Hostname:</strong> " + deviceHostname + "</p>";
    html += "<p><strong>Link Status:</strong> " + String(ETH.linkUp() ? "Up" : "Down") + "</p>";
    html += "<p><strong>SIP Status:</strong> " + String(sipRegistered ? "Registered" : "Not Registered") + "</p>";
    
    // LED status with description
    String ledStatus;
    switch (currentLedMode) {
      case LED_IDLE:
        ledStatus = "Idle (Green)";
        break;
      case LED_INCOMING_CALL:
        ledStatus = "Incoming Call (Red/Orange Flash)";
        break;
      case LED_SIP_ERROR:
        ledStatus = "SIP Error (Red Blink)";
        break;
    }
    html += "<p><strong>LED Status:</strong> " + ledStatus + "</p>";
    html += "</div>";
    
    // Configuration form
    html += "<h2>Configuration</h2>";
    html += "<form action='/save' method='POST'>";
    html += "<p><label>Hostname:</label><input type='text' name='hostname' value='" + deviceHostname + "'></p>";
    html += "<p><label>SIP Server:</label><input type='text' name='sipServer' value='" + sipServer + "'></p>";
    html += "<p><label>SIP Port:</label><input type='number' name='sipPort' value='" + String(sipPort) + "'></p>";
    html += "<p><label>SIP Username:</label><input type='text' name='sipUsername' value='" + sipUsername + "'></p>";
    html += "<p><label>SIP Password:</label><input type='password' name='sipPassword' value='" + sipPassword + "'></p>";
    html += "<p><label>SIP Realm:</label><input type='text' name='sipRealm' value='" + sipRealm + "'></p>";
    html += "<p><input type='submit' value='Save Configuration'></p>";
    html += "</form>";
    
    // Register button
    html += "<h2>Actions</h2>";
    html += "<form action='/register' method='POST'>";
    html += "<input type='submit' value='Register to SIP Server'>";
    html += "</form>";
    
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  
  // Save configuration
  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("hostname")) deviceHostname = server.arg("hostname");
    if (server.hasArg("sipServer")) sipServer = server.arg("sipServer");
    if (server.hasArg("sipPort")) sipPort = server.arg("sipPort").toInt();
    if (server.hasArg("sipUsername")) sipUsername = server.arg("sipUsername");
    if (server.hasArg("sipPassword")) sipPassword = server.arg("sipPassword");
    if (server.hasArg("sipRealm")) sipRealm = server.arg("sipRealm");
    
    saveConfiguration();
    
    ETH.setHostname(deviceHostname.c_str());
    
    server.sendHeader("Location", "/");
    server.send(303);
  });
  
  // Manual SIP registration
  server.on("/register", HTTP_POST, []() {
    registerSIP();
    server.sendHeader("Location", "/");
    server.send(303);
  });
  
  server.begin();
  Serial.println("Web server started on port 80");
}

void sendLLDP() {
  if (!ETH.linkUp() || ethIPAddress.isEmpty() || eth_handle == NULL) {
    return;
  }
  
  Serial.println("Sending LLDP frame...");
  
  // Prepare LLDP frame buffer (max 1500 bytes)
  uint8_t lldpFrame[512];
  int framePos = 0;
  
  // Ethernet Header
  // Destination MAC: LLDP multicast address 01:80:c2:00:00:0e
  lldpFrame[framePos++] = 0x01;
  lldpFrame[framePos++] = 0x80;
  lldpFrame[framePos++] = 0xc2;
  lldpFrame[framePos++] = 0x00;
  lldpFrame[framePos++] = 0x00;
  lldpFrame[framePos++] = 0x0e;
  
  // Source MAC: Our MAC address
  uint8_t mac[6];
  sscanf(ethMACAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
         &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  for (int i = 0; i < 6; i++) {
    lldpFrame[framePos++] = mac[i];
  }
  
  // EtherType: LLDP (0x88cc)
  lldpFrame[framePos++] = 0x88;
  lldpFrame[framePos++] = 0xcc;
  
  // LLDP Payload starts here
  
  // TLV 1: Chassis ID (Type=1)
  // Format: Type(7 bits) | Length(9 bits) | Subtype(1 byte) | Value
  lldpFrame[framePos++] = 0x02;  // Type=1, Length=7 (high 7 bits of 0x02 = type 1)
  lldpFrame[framePos++] = 0x07;  // Length = 7 bytes (1 subtype + 6 MAC)
  lldpFrame[framePos++] = 0x04;  // Subtype: MAC address (4)
  // MAC address
  for (int i = 0; i < 6; i++) {
    lldpFrame[framePos++] = mac[i];
  }
  
  // TLV 2: Port ID (Type=2)
  String portID = "eth0";
  uint8_t portIDLen = portID.length() + 1; // +1 for subtype
  lldpFrame[framePos++] = 0x04;  // Type=2, length high bits
  lldpFrame[framePos++] = portIDLen;  // Length
  lldpFrame[framePos++] = 0x05;  // Subtype: Interface name (5)
  for (unsigned int i = 0; i < portID.length(); i++) {
    lldpFrame[framePos++] = portID[i];
  }
  
  // TLV 3: TTL (Type=3)
  lldpFrame[framePos++] = 0x06;  // Type=3, length high bits
  lldpFrame[framePos++] = 0x02;  // Length = 2 bytes
  lldpFrame[framePos++] = 0x00;  // TTL high byte (120 seconds)
  lldpFrame[framePos++] = 0x78;  // TTL low byte (120 seconds)
  
  // TLV 5: System Name (Type=5)
  uint8_t hostnameLen = deviceHostname.length();
  lldpFrame[framePos++] = 0x0a;  // Type=5, length high bits
  lldpFrame[framePos++] = hostnameLen;  // Length
  for (unsigned int i = 0; i < deviceHostname.length(); i++) {
    lldpFrame[framePos++] = deviceHostname[i];
  }
  
  // TLV 6: System Description (Type=6) - Optional
  String sysDesc = "ESP32 SIP/LLDP Device";
  uint8_t sysDescLen = sysDesc.length();
  lldpFrame[framePos++] = 0x0c;  // Type=6, length high bits
  lldpFrame[framePos++] = sysDescLen;  // Length
  for (unsigned int i = 0; i < sysDesc.length(); i++) {
    lldpFrame[framePos++] = sysDesc[i];
  }
  
  // TLV 8: Management Address (Type=8)
  IPAddress localIP = ETH.localIP();
  // Address String Length (1) + Address Subtype (1) + Address (4) + 
  // Interface Subtype (1) + Interface Number (4) + OID Length (1)
  lldpFrame[framePos++] = 0x10;  // Type=8, length high bits
  lldpFrame[framePos++] = 0x0c;  // Length = 12 bytes
  lldpFrame[framePos++] = 0x05;  // Address string length = 5 (1 subtype + 4 IP)
  lldpFrame[framePos++] = 0x01;  // Address subtype: IPv4 (1)
  lldpFrame[framePos++] = localIP[0];  // IP address byte 1
  lldpFrame[framePos++] = localIP[1];  // IP address byte 2
  lldpFrame[framePos++] = localIP[2];  // IP address byte 3
  lldpFrame[framePos++] = localIP[3];  // IP address byte 4
  lldpFrame[framePos++] = 0x01;  // Interface numbering subtype: ifIndex (1)
  lldpFrame[framePos++] = 0x00;  // Interface number (4 bytes) = 1
  lldpFrame[framePos++] = 0x00;
  lldpFrame[framePos++] = 0x00;
  lldpFrame[framePos++] = 0x01;
  lldpFrame[framePos++] = 0x00;  // OID string length = 0 (no OID)
  
  // TLV 0: End of LLDPDU (Type=0, Length=0)
  lldpFrame[framePos++] = 0x00;
  lldpFrame[framePos++] = 0x00;
  
  // Send the raw Ethernet frame
  esp_err_t err = esp_eth_transmit(eth_handle, lldpFrame, framePos);
  
  if (err == ESP_OK) {
    Serial.println("LLDP frame sent successfully (" + String(framePos) + " bytes)");
  } else {
    Serial.println("LLDP frame send failed: " + String(err));
  }
}

String generateCallID() {
  String localIP = ETH.localIP().toString();
  return String(random(100000000)) + "@" + localIP;
}

String generateTag() {
  return String(random(100000000));
}

String calculateMD5(String input) {
  MD5Builder md5;
  md5.begin();
  md5.add(input);
  md5.calculate();
  return md5.toString();
}

String extractParameter(String message, String startDelim, String endDelim) {
  int startIdx = message.indexOf(startDelim);
  if (startIdx == -1) return "";
  
  startIdx += startDelim.length();
  int endIdx = message.indexOf(endDelim, startIdx);
  if (endIdx == -1) {
    // If endDelim not found, take rest of string (may need trimming)
    String result = message.substring(startIdx);
    result.trim();
    return result;
  }
  
  String result = message.substring(startIdx, endIdx);
  result.trim();
  return result;
}

void registerSIP() {
  if (sipServer.isEmpty() || sipUsername.isEmpty()) {
    Serial.println("SIP configuration incomplete!");
    return;
  }
  
  Serial.println("Registering to SIP server...");
  
  // Get current IP
  String localIP = ETH.localIP().toString();
  
  // Generate call ID and tag
  String callID = generateCallID();
  String fromTag = generateTag();
  String branch = "z9hG4bK" + generateTag();
  
  // Build REGISTER request (RFC 3261 compliant)
  String registerMsg = "REGISTER sip:" + sipServer + " SIP/2.0\r\n";
  registerMsg += "Via: SIP/2.0/UDP " + localIP + ":" + String(sipPort) + ";branch=" + branch + ";rport\r\n";
  registerMsg += "Max-Forwards: 70\r\n";
  registerMsg += "From: <sip:" + sipUsername + "@" + sipServer + ">;tag=" + fromTag + "\r\n";
  registerMsg += "To: <sip:" + sipUsername + "@" + sipServer + ">\r\n";
  registerMsg += "Call-ID: " + callID + "\r\n";
  registerMsg += "CSeq: 1 REGISTER\r\n";
  registerMsg += "Contact: <sip:" + sipUsername + "@" + localIP + ":" + String(sipPort) + ">\r\n";
  registerMsg += "Allow: INVITE, ACK, CANCEL, BYE, NOTIFY, REFER, MESSAGE, OPTIONS, INFO, SUBSCRIBE\r\n";
  registerMsg += "Expires: 3600\r\n";
  registerMsg += "User-Agent: ";
  registerMsg += SIP_USER_AGENT;
  registerMsg += "\r\n";
  registerMsg += "Content-Length: 0\r\n";
  registerMsg += "\r\n";
  
  // Send REGISTER
  udpSIP.beginPacket(sipServer.c_str(), sipPort);
  udpSIP.write((const uint8_t*)registerMsg.c_str(), registerMsg.length());
  udpSIP.endPacket();
  
  lastRegisterTime = millis();
  
  Serial.println("REGISTER sent:");
  Serial.println(registerMsg);
}

void sendSIPResponse(String remoteIP, int remotePort, String response) {
  udpSIP.beginPacket(remoteIP.c_str(), remotePort);
  udpSIP.write((const uint8_t*)response.c_str(), response.length());
  udpSIP.endPacket();
  Serial.println("SIP Response sent:");
  Serial.println(response);
}

void handleAuthChallenge(String message, String remoteIP, int remotePort) {
  // Extract nonce and realm from challenge
  String nonce = extractParameter(message, "nonce=\"", "\"");
  String realm = extractParameter(message, "realm=\"", "\"");
  String algorithm = extractParameter(message, "algorithm=", ",");
  if (algorithm.isEmpty()) {
    algorithm = extractParameter(message, "algorithm=", "\r");
  }
  // Clean up algorithm - remove quotes and whitespace
  algorithm.trim();
  algorithm.replace("\"", "");
  if (algorithm.isEmpty()) {
    algorithm = "MD5";  // Default to MD5
  }
  
  // Extract Call-ID, From tag, and CSeq from the 401 response
  String callID = extractParameter(message, "Call-ID: ", "\r\n");
  callID.trim();
  
  String fromHeader = extractParameter(message, "From: ", "\r\n");
  String fromTag = extractParameter(fromHeader, ";tag=", "\r");
  if (fromTag.indexOf(">") > 0) {
    fromTag = fromTag.substring(0, fromTag.indexOf(">"));
  }
  if (fromTag.indexOf(",") > 0) {
    fromTag = fromTag.substring(0, fromTag.indexOf(","));
  }
  fromTag.trim();
  
  String cseqLine = extractParameter(message, "CSeq: ", "\r\n");
  String cseqNum = cseqLine.substring(0, cseqLine.indexOf(" "));
  int cseq = cseqNum.toInt();
  
  if (realm.isEmpty() && !sipRealm.isEmpty()) {
    realm = sipRealm;
  }
  
  Serial.println("=== Authentication Challenge ===");
  Serial.println("Nonce: " + nonce);
  Serial.println("Realm: " + realm);
  Serial.println("Algorithm: " + algorithm);
  Serial.println("Call-ID: " + callID);
  Serial.println("From Tag: " + fromTag);
  Serial.println("CSeq: " + String(cseq));
  Serial.println("==============================");
  
  // Get current IP
  String localIP = ETH.localIP().toString();
  
  // Calculate digest response (RFC 2617)
  String uri = "sip:" + sipServer;
  String ha1 = calculateMD5(sipUsername + ":" + realm + ":" + sipPassword);
  String ha2 = calculateMD5("REGISTER:" + uri);
  String response = calculateMD5(ha1 + ":" + nonce + ":" + ha2);
  
  Serial.println("HA1: " + ha1);
  Serial.println("HA2: " + ha2);
  Serial.println("Response: " + response);
  
  // Generate new branch for this transaction
  String branch = "z9hG4bK" + generateTag();
  
  // Increment CSeq
  cseq++;
  
  // Build authenticated REGISTER with same Call-ID and From tag
  String registerMsg = "REGISTER sip:" + sipServer + " SIP/2.0\r\n";
  registerMsg += "Via: SIP/2.0/UDP " + localIP + ":" + String(sipPort) + ";branch=" + branch + ";rport\r\n";
  registerMsg += "Max-Forwards: 70\r\n";
  registerMsg += "From: <sip:" + sipUsername + "@" + sipServer + ">;tag=" + fromTag + "\r\n";
  registerMsg += "To: <sip:" + sipUsername + "@" + sipServer + ">\r\n";
  registerMsg += "Call-ID: " + callID + "\r\n";
  registerMsg += "CSeq: " + String(cseq) + " REGISTER\r\n";
  registerMsg += "Contact: <sip:" + sipUsername + "@" + localIP + ":" + String(sipPort) + ">\r\n";
  
  // Build Authorization header with all parameters properly quoted
  registerMsg += "Authorization: Digest ";
  registerMsg += "username=\"" + sipUsername + "\", ";
  registerMsg += "realm=\"" + realm + "\", ";
  registerMsg += "nonce=\"" + nonce + "\", ";
  registerMsg += "uri=\"" + uri + "\", ";
  registerMsg += "response=\"" + response + "\", ";
  registerMsg += "algorithm=" + algorithm + "\r\n";
  
  registerMsg += "Allow: INVITE, ACK, CANCEL, BYE, NOTIFY, REFER, MESSAGE, OPTIONS, INFO, SUBSCRIBE\r\n";
  registerMsg += "Expires: 3600\r\n";
  registerMsg += "User-Agent: ";
  registerMsg += SIP_USER_AGENT;
  registerMsg += "\r\n";
  registerMsg += "Content-Length: 0\r\n";
  registerMsg += "\r\n";
  
  Serial.println("=== Sending Authenticated REGISTER ===");
  Serial.println(registerMsg);
  Serial.println("=====================================");
  
  // Send authenticated REGISTER
  sendSIPResponse(remoteIP, remotePort, registerMsg);
}

void handleInvite(String message, String remoteIP, int remotePort) {
  // Extract call details
  currentCallID = extractParameter(message, "Call-ID: ", "\r\n");
  currentFromTag = extractParameter(message, "From: ", ";tag=");
  currentFromTag = extractParameter(currentFromTag + message, ";tag=", "\r\n");
  String cseq = extractParameter(message, "CSeq: ", "\r\n");
  String via = extractParameter(message, "Via: ", "\r\n");
  String from = extractParameter(message, "From: ", "\r\n");
  String to = extractParameter(message, "To: ", "\r\n");
  String contact = extractParameter(message, "Contact: ", "\r\n");
  
  // Generate To tag
  currentToTag = generateTag();
  
  // Get current IP
  String localIP = ETH.localIP().toString();
  
  // Send 180 Ringing
  String ringing = "SIP/2.0 180 Ringing\r\n";
  ringing += "Via: " + via + "\r\n";
  ringing += "From: " + from + "\r\n";
  ringing += "To: " + to + ";tag=" + currentToTag + "\r\n";
  ringing += "Call-ID: " + currentCallID + "\r\n";
  ringing += "CSeq: " + cseq + "\r\n";
  ringing += "Contact: <sip:" + sipUsername + "@" + localIP + ":" + String(sipPort) + ">\r\n";
  ringing += "Content-Length: 0\r\n";
  ringing += "\r\n";
  
  sendSIPResponse(remoteIP, remotePort, ringing);
}

void handleBye(String message, String remoteIP, int remotePort) {
  String cseq = extractParameter(message, "CSeq: ", "\r\n");
  String via = extractParameter(message, "Via: ", "\r\n");
  String from = extractParameter(message, "From: ", "\r\n");
  String to = extractParameter(message, "To: ", "\r\n");
  String callID = extractParameter(message, "Call-ID: ", "\r\n");
  
  // Send 200 OK
  String ok = "SIP/2.0 200 OK\r\n";
  ok += "Via: " + via + "\r\n";
  ok += "From: " + from + "\r\n";
  ok += "To: " + to + "\r\n";
  ok += "Call-ID: " + callID + "\r\n";
  ok += "CSeq: " + cseq + "\r\n";
  ok += "Content-Length: 0\r\n";
  ok += "\r\n";
  
  sendSIPResponse(remoteIP, remotePort, ok);
  
  // Clear call state
  currentCallID = "";
  currentFromTag = "";
  currentToTag = "";
}

void handleOptions(String message, String remoteIP, int remotePort) {
  // Extract headers from OPTIONS request
  String cseq = extractParameter(message, "CSeq: ", "\r\n");
  String via = extractParameter(message, "Via: ", "\r\n");
  String from = extractParameter(message, "From: ", "\r\n");
  String to = extractParameter(message, "To: ", "\r\n");
  String callID = extractParameter(message, "Call-ID: ", "\r\n");
  
  // Get current IP
  String localIP = ETH.localIP().toString();
  
  // Build 200 OK response with supported methods
  String ok = "SIP/2.0 200 OK\r\n";
  ok += "Via: " + via + "\r\n";
  ok += "From: " + from + "\r\n";
  ok += "To: " + to + "\r\n";
  ok += "Call-ID: " + callID + "\r\n";
  ok += "CSeq: " + cseq + "\r\n";
  ok += "Contact: <sip:" + sipUsername + "@" + localIP + ":" + String(sipPort) + ">\r\n";
  ok += "Accept: application/sdp\r\n";
  ok += "Accept-Language: en\r\n";
  ok += "Allow: INVITE, CANCEL, BYE, OPTIONS\r\n";
  ok += "Supported: replaces, timer\r\n";
  ok += "User-Agent: ";
  ok += SIP_USER_AGENT;
  ok += "\r\n";
  ok += "Content-Length: 0\r\n";
  ok += "\r\n";
  
  sendSIPResponse(remoteIP, remotePort, ok);
  
  Serial.println("OPTIONS 200 OK sent");
}

void handleSIP() {
  int packetSize = udpSIP.parsePacket();
  if (packetSize > 0) {
    char incomingPacket[2048];
    int len = udpSIP.read(incomingPacket, sizeof(incomingPacket) - 1);
    if (len > 0) {
      incomingPacket[len] = 0;
    }
    
    String sipMessage = String(incomingPacket);
    String remoteIP = udpSIP.remoteIP().toString();
    int remotePort = udpSIP.remotePort();
    
    Serial.println("\n=== Received SIP Message ===");
    Serial.println(sipMessage);
    Serial.println("============================\n");
    
    // Parse SIP message
    if (sipMessage.startsWith("SIP/2.0 401")) {
      // Authentication required
      Serial.println("Authentication challenge received");
      
      // Protect against infinite auth loops
      if (millis() - lastAuthAttempt < 5000 && authAttempts >= 3) {
        Serial.println("ERROR: Too many authentication failures - stopping");
        authAttempts = 0;
        return;
      }
      
      authAttempts++;
      lastAuthAttempt = millis();
      handleAuthChallenge(sipMessage, remoteIP, remotePort);
    }
    else if (sipMessage.startsWith("SIP/2.0 200")) {
      // Success response
      if (sipMessage.indexOf("REGISTER") > 0 || sipMessage.indexOf("CSeq:") > 0) {
        sipRegistered = true;
        authAttempts = 0;  // Reset auth attempts on success
        Serial.println("SIP registration successful!");
      }
    }
    else if (sipMessage.startsWith("INVITE")) {
      // Incoming call!
      Serial.println("*** INCOMING CALL ***");
      currentLedMode = LED_INCOMING_CALL;
      
      handleInvite(sipMessage, remoteIP, remotePort);
    }
    else if (sipMessage.startsWith("BYE") || sipMessage.startsWith("CANCEL")) {
      // Call ended
      Serial.println("*** CALL ENDED ***");
      
      handleBye(sipMessage, remoteIP, remotePort);
    }
    else if (sipMessage.startsWith("OPTIONS")) {
      // OPTIONS request - respond with capabilities
      Serial.println("OPTIONS request received");
      handleOptions(sipMessage, remoteIP, remotePort);
    }
  }
}

void initOTA() {
  ArduinoOTA.setHostname(deviceHostname.c_str());
  ArduinoOTA.setMdnsEnabled(mdnsEnabled);
  ArduinoOTA.begin();
}

void initMDNS() {
  if (mdnsEnabled) {
    MDNS.begin(deviceHostname.c_str());
    MDNS.addService("http", "_tcp", 80);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("SIP Alerter is starting...");

  strip.begin();
  strip.setBrightness(25);
  updateLEDs();

  loadConfiguration();

  initEthernet();

  initMDNS();

  initOTA();

  initWebServer();

  udpSIP.begin(sipPort);

  // Wait a bit for Ethernet to fully initialize
  delay(500);

  eth_handle = getEthHandle();
  if (eth_handle == NULL) {
    Serial.println("Warning: Could not get Ethernet handle for LLDP");
    Serial.println("LLDP will not be available");
  } else {
    Serial.println("Ethernet handle obtained successfully for LLDP");
  }
  
  Serial.println("Setup complete!");

  if (sipServer != "") {
    registerSIP();
  }
}

void loop() {
  // Handle web server
  server.handleClient();
 
  // Handle OTA
  ArduinoOTA.handle();
  
  // Handle SIP messages
  handleSIP();
  
  // Send LLDP broadcasts periodically
  if (lldpEnabled && ((millis() - lastLLDPTime) > lldpInterval)) {
    sendLLDP();
    lastLLDPTime = millis();
  }
  
  // Re-register to SIP server periodically
  if (sipRegistered && millis() - lastRegisterTime > registerInterval) {
    registerSIP();
  }
  
  // Update LED status
  updateLEDs();
}
