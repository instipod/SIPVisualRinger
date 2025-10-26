#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <Adafruit_NeoPixel.h>
#include <sip.h>
// ESP-IDF includes for raw Ethernet frames
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
// mbedtls for AES encryption
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

// Build configuration
#define WS2811_PIN 12
#define WS2811_COUNT 3
#define ETH_PHY_ADDR 1
#define ETH_PHY_MDC 23
#define ETH_PHY_MDIO 18
#define ETH_PHY_POWER 16
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
#define ETH_PHY_TYPE ETH_PHY_LAN8720
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
String deviceHostname = "VisualAlert-ffff";
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
SIPClient sipLineOne = SIPClient(5060);
SIPClient sipLineTwo = SIPClient(5061);

// Web authentication variables
String webUsername = "admin";
String webPassword = "admin";
const unsigned long SESSION_TIMEOUT = 3600000; // 1 hour in milliseconds
const String AUTH_SECRET = "SIPVisualRinger2024"; // Secret key for cookie encryption

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

  sipLineOne.end_registration(true);
  sipLineTwo.end_registration(true);
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
  } else if (sipLineOne.is_ringing() && currentLedMode == LED_INCOMING_CALL) {
    targetMode = LED_INCOMING_CALL;
    ledFlashInterval = LED_FAST_FLASH;
  } else if (!sipLineOne.is_registered() && !sipServer.isEmpty()) {
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

  deviceHostname = preferences.getString("hostname", "VisualAlert-" + ethMACAddress.substring(ethMACAddress.length() - 5, ethMACAddress.length() - 1));
  sipServer = preferences.getString("sipServer", "");
  sipPort = preferences.getInt("sipPort", 5060);
  sipUsername = preferences.getString("sipUser", "");
  sipPassword = preferences.getString("sipPass", "");
  sipRealm = preferences.getString("sipRealm", "");
  webUsername = preferences.getString("webUser", "admin");
  webPassword = preferences.getString("webPass", "admin");

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
  preferences.putString("webUser", webUsername);
  preferences.putString("webPass", webPassword);

  preferences.end();

  Serial.println("Configuration saved!");
}

// AES encryption for cookie with random IV
String encryptCookie(String data) {
  // Pad data to multiple of 16 bytes (AES block size)
  int paddingLength = 16 - (data.length() % 16);
  for (int i = 0; i < paddingLength; i++) {
    data += (char)paddingLength;
  }

  unsigned char key[32];
  // Create 256-bit key from secret
  for (int i = 0; i < 32; i++) {
    key[i] = AUTH_SECRET[i % AUTH_SECRET.length()] ^ (i * 7);
  }

  // Generate random IV (16 bytes)
  unsigned char iv[16];
  for (int i = 0; i < 16; i++) {
    iv[i] = random(0, 256);
  }

  unsigned char encrypted[256];

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 256);

  // Encrypt
  const unsigned char* input = (const unsigned char*)data.c_str();
  unsigned char iv_copy[16];
  memcpy(iv_copy, iv, 16); // CBC mode modifies IV, so use a copy

  for (size_t i = 0; i < data.length(); i += 16) {
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, 16, iv_copy, input + i, encrypted + i);
  }

  mbedtls_aes_free(&aes);

  // Combine IV + encrypted data
  unsigned char combined[272]; // 16 bytes IV + 256 bytes max encrypted data
  memcpy(combined, iv, 16);
  memcpy(combined + 16, encrypted, data.length());

  // Base64 encode the combined IV + encrypted data
  unsigned char base64Output[512];
  size_t outputLen;
  mbedtls_base64_encode(base64Output, sizeof(base64Output), &outputLen, combined, 16 + data.length());

  return String((char*)base64Output);
}

// AES decryption for cookie with IV extraction
String decryptCookie(String data) {
  // Base64 decode
  unsigned char decoded[512];
  size_t decodedLen;
  int ret = mbedtls_base64_decode(decoded, sizeof(decoded), &decodedLen,
                                    (const unsigned char*)data.c_str(), data.length());
  if (ret != 0 || decodedLen < 16) {
    return ""; // Need at least 16 bytes for IV
  }

  unsigned char key[32];
  // Create same 256-bit key from secret
  for (int i = 0; i < 32; i++) {
    key[i] = AUTH_SECRET[i % AUTH_SECRET.length()] ^ (i * 7);
  }

  // Extract IV from first 16 bytes
  unsigned char iv[16];
  memcpy(iv, decoded, 16);

  // Encrypted data starts after IV
  unsigned char* encryptedData = decoded + 16;
  size_t encryptedLen = decodedLen - 16;

  unsigned char output[256];

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, key, 256);

  // Decrypt
  for (size_t i = 0; i < encryptedLen; i += 16) {
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv, encryptedData + i, output + i);
  }

  mbedtls_aes_free(&aes);

  // Remove padding
  int paddingLength = output[encryptedLen - 1];
  if (paddingLength > 0 && paddingLength <= 16) {
    encryptedLen -= paddingLength;
  }

  output[encryptedLen] = '\0';
  return String((char*)output);
}

// Create authentication cookie with expiry timestamp
String createAuthCookie() {
  unsigned long expiryTime = millis() + SESSION_TIMEOUT;
  String cookieData = String(expiryTime) + "|authenticated";
  return encryptCookie(cookieData);
}

// Check if user is authenticated via cookie
bool isAuthenticated() {
  if (!server.hasHeader("Cookie")) {
    return false;
  }

  String cookie = server.header("Cookie");
  int authStart = cookie.indexOf("auth=");
  if (authStart == -1) {
    return false;
  }

  authStart += 5; // Skip "auth="
  int authEnd = cookie.indexOf(';', authStart);
  String authCookie;
  if (authEnd == -1) {
    authCookie = cookie.substring(authStart);
  } else {
    authCookie = cookie.substring(authStart, authEnd);
  }

  // URL decode the cookie value (replace %XX with actual characters)
  authCookie.replace("%2B", "+");
  authCookie.replace("%2F", "/");
  authCookie.replace("%3D", "=");

  // Decrypt and validate cookie
  String decrypted = decryptCookie(authCookie);
  if (decrypted.length() == 0) {
    return false;
  }

  int separatorPos = decrypted.indexOf('|');
  if (separatorPos == -1) {
    return false;
  }

  unsigned long expiryTime = decrypted.substring(0, separatorPos).toInt();
  String authStatus = decrypted.substring(separatorPos + 1);

  // Check if cookie is valid and not expired
  if (authStatus == "authenticated" && millis() < expiryTime) {
    return true;
  }

  return false;
}

// Redirect to login page
void redirectToLogin() {
  server.sendHeader("Location", "/login");
  server.send(303);
}

void initWebServer() {
  // Login page
  server.on("/login", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>Login - ESP32 SIP Device</title>";
    html += "<style>body{font-family:Arial;margin:0;padding:0;display:flex;justify-content:center;align-items:center;height:100vh;background:#f0f0f0;} ";
    html += ".login-box{background:white;padding:40px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);width:300px;} ";
    html += "h1{text-align:center;color:#333;margin-bottom:30px;} ";
    html += "input[type=text],input[type=password]{width:100%;padding:12px;margin:10px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;} ";
    html += "input[type=submit]{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin-top:10px;} ";
    html += "input[type=submit]:hover{background:#45a049;} ";
    html += "label{color:#666;font-size:14px;} ";
    html += ".error{color:red;text-align:center;margin-bottom:10px;}</style>";
    html += "</head><body>";
    html += "<div class='login-box'>";
    html += "<h1>ESP32 SIP Device</h1>";

    // Show error message if login failed
    if (server.hasArg("error")) {
      html += "<div class='error'>Invalid username or password</div>";
    }

    html += "<form action='/login' method='POST'>";
    html += "<label>Username:</label>";
    html += "<input type='text' name='username' required autofocus>";
    html += "<label>Password:</label>";
    html += "<input type='password' name='password' required>";
    html += "<input type='submit' value='Login'>";
    html += "</form>";
    html += "</div>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  // Login POST handler
  server.on("/login", HTTP_POST, []() {
    String username = server.arg("username");
    String password = server.arg("password");

    if (username == webUsername && password == webPassword) {
      // Create encrypted auth cookie
      String authCookie = createAuthCookie();

      // Set cookie with HttpOnly flag (as much as possible in this library)
      String cookieHeader = "auth=" + authCookie + "; Path=/; Max-Age=3600; SameSite=Strict";
      server.sendHeader("Set-Cookie", cookieHeader);
      server.sendHeader("Location", "/");
      server.send(303);

      Serial.println("User logged in successfully");
    } else {
      // Redirect back to login with error
      server.sendHeader("Location", "/login?error=1");
      server.send(303);

      Serial.println("Failed login attempt - username: " + username);
    }
  });

  // Logout handler
  server.on("/logout", HTTP_GET, []() {
    // Clear the auth cookie
    server.sendHeader("Set-Cookie", "auth=; Path=/; Max-Age=0");
    server.sendHeader("Location", "/login");
    server.send(303);

    Serial.println("User logged out");
  });

  // Root page - configuration interface
  server.on("/", HTTP_GET, []() {
    // Check authentication
    if (!isAuthenticated()) {
      redirectToLogin();
      return;
    }

    // Get current IP address dynamically
    String currentIP = ETH.localIP().toString();
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>ESP32 SIP/LLDP Configuration</title>";
    html += "<style>body{font-family:Arial;margin:20px;} ";
    html += "input[type=text],input[type=number],input[type=password]{width:300px;padding:8px;margin:5px 0;} ";
    html += "input[type=submit]{padding:10px 20px;background:#4CAF50;color:white;border:none;cursor:pointer;} ";
    html += ".logout-btn{float:right;padding:10px 20px;background:#f44336;color:white;border:none;cursor:pointer;text-decoration:none;border-radius:5px;} ";
    html += "label{display:inline-block;width:150px;} ";
    html += ".status{background:#f0f0f0;padding:15px;margin:20px 0;border-radius:5px;} ";
    html += "header{overflow:auto;margin-bottom:20px;}</style>";
    html += "</head><body>";
    html += "<header><h1 style='float:left;margin:0;'>ESP32 SIP/LLDP Device</h1>";
    html += "<a href='/logout' class='logout-btn'>Logout</a></header><div style='clear:both;'></div>";
    
    // Status section
    html += "<div class='status'>";
    html += "<h2>Status</h2>";
    html += "<p><strong>IP Address:</strong> " + currentIP + "</p>";
    html += "<p><strong>MAC Address:</strong> " + ethMACAddress + "</p>";
    html += "<p><strong>Hostname:</strong> " + deviceHostname + "</p>";
    html += "<p><strong>Link Status:</strong> " + String(ETH.linkUp() ? "Up" : "Down") + "</p>";
    html += "<p><strong>SIP Status:</strong> " + String(sipLineOne.is_registered() ? "Registered" : "Not Registered") + "</p>";
    
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
    // Check authentication
    if (!isAuthenticated()) {
      redirectToLogin();
      return;
    }

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
    // Check authentication
    if (!isAuthenticated()) {
      redirectToLogin();
      return;
    }

    sipLineOne.begin_registration();
    sipLineTwo.begin_registration();
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

void initMDNS() {
  if (mdnsEnabled) {
    MDNS.begin(deviceHostname.c_str());
    MDNS.addService("http", "_tcp", 80);
    MDNS.addService("visualalert", "_tcp", 80);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  ethMACAddress = ETH.macAddress();
  deviceHostname = "VisualAlert-" + ethMACAddress.substring(ethMACAddress.length() - 5, ethMACAddress.length() - 1);

  Serial.println("SIP Alerter is starting...");

  strip.begin();
  strip.setBrightness(25);
  updateLEDs();

  loadConfiguration();

  initEthernet();

  initMDNS();

  initWebServer();

  sipLineOne.update_credentials(sipServer, sipPort, sipUsername, sipPassword, sipRealm);

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
    sipLineOne.begin_registration();
    sipLineTwo.begin_registration();
  }
}

void loop() {
  // Handle web server
  server.handleClient();
  
  // Handle SIP messages
  sipLineOne.handle();
  sipLineTwo.handle();
  
  // Send LLDP broadcasts periodically
  if (lldpEnabled && ((millis() - lastLLDPTime) > lldpInterval)) {
    sendLLDP();
    lastLLDPTime = millis();
  }
  
  // Update LED status
  updateLEDs();
}
