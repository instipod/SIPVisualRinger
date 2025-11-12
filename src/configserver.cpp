#include "configserver.h"
#include "webpages.h"

String ConfigServer::encrypt_cookie(String data) {
  // Pad data to multiple of 16 bytes (AES block size)
  int paddingLength = 16 - (data.length() % 16);
  for (int i = 0; i < paddingLength; i++) {
    data += (char)paddingLength;
  }

  unsigned char key[32];
  // Create 256-bit key from secret
  for (int i = 0; i < 32; i++) {
    key[i] = authSecret[i % authSecret.length()] ^ (i * 7);
  }

  // Generate cryptographically secure random IV (16 bytes)
  unsigned char iv[16];
  for (int i = 0; i < 16; i++) {
    iv[i] = runtime.get_srandom_byte();
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
String ConfigServer::decrypt_cookie(String data) {
  // Base64 decode
  unsigned char decoded[512];
  size_t decodedLen;

  int ret = mbedtls_base64_decode(decoded, sizeof(decoded), &decodedLen,
                                    (const unsigned char*)data.c_str(), data.length());
  if (ret != 0 || decodedLen < 16) {
    Serial.println("Did not have enough data for an IV, invalid cookie.");
    return ""; // Need at least 16 bytes for IV
  }

  unsigned char key[32];
  // Create same 256-bit key from secret
  for (int i = 0; i < 32; i++) {
    key[i] = authSecret[i % authSecret.length()] ^ (i * 7);
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
String ConfigServer::create_auth_cookie(String username) {
  unsigned long expiryTime = millis() + sessionTimeout;
  String cookieData = String(expiryTime) + "|authenticated|" + username;
  return encrypt_cookie(cookieData);
}

// Check if user is authenticated via cookie
bool ConfigServer::is_authenticated() {
  if (!server.hasHeader("cookie") && !server.hasHeader("Cookie")) {
    return false;
  }

  String cookie = server.header("cookie");
  if (cookie.isEmpty()) {
    cookie = server.header("Cookie");
  }
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
  String decrypted = decrypt_cookie(authCookie);
  if (decrypted.length() == 0) {
    return false;
  }

  int separatorPos = decrypted.indexOf('|');
  if (separatorPos == -1) {
    return false;
  }

  String parts[3];
  int partCount = runtime.split_string(decrypted, '|', parts, 3);
  if (partCount < 3) {
    return false;
  }

  unsigned long expiryTime = parts[0].toInt();
  String authStatus = parts[1];
  String username = parts[2];

  // Check if cookie is valid and not expired
  if (authStatus == "authenticated" && millis() < expiryTime) {
    Serial.println("User " + username + " validated from cookie. [Expires: " + parts[0] + "]");
    return true;
  } else {
    Serial.println("User " + username + " failed validation with expired cookie.");
  }

  return false;
}

// Redirect to login page
void ConfigServer::redirect_to_login() {
  server.sendHeader("Location", "/login");
  server.send(303);
}

void ConfigServer::init() {
  authSecret = runtime.get_random_string(32);
  Serial.println("Initialized config server authentication with secret " + authSecret);

  // Tell the server to collect Cookie headers
  const char* headerKeys[] = {"Cookie"};
  server.collectHeaders(headerKeys, 1);

  // CSS file
  server.on("/output.css", HTTP_GET, [&]() {
    server.send(200, "text/css", css_output);
  });

  // Login page
  server.on("/login", HTTP_GET, [&]() {
    // Send response headers first
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    // Process HTML from PROGMEM in chunks
    const size_t CHUNK_SIZE = 512;
    char buffer[CHUNK_SIZE + 1];
    size_t len = strlen_P(webpage_login);
    size_t pos = 0;

    while (pos < len) {
      size_t chunk = min(CHUNK_SIZE, len - pos);
      memcpy_P(buffer, webpage_login + pos, chunk);
      buffer[chunk] = '\0';

      String htmlChunk = String(buffer);
      htmlChunk.replace("{HOSTNAME}", runtime.deviceHostname);
      htmlChunk.replace("{MAC_ADDRESS}", runtime.get_ethernet_mac_address());

      server.sendContent(htmlChunk);
      pos += chunk;
    }

    server.sendContent(""); // Signal end of content
  });

  // Login POST handler
  server.on("/login", HTTP_POST, [&]() {
    String username = server.arg("username");
    String password = server.arg("password");

    if (username == "admin" && password == runtime.webPassword) {
      // Create encrypted auth cookie
      String authCookie = create_auth_cookie(username);

      // Set cookie with HttpOnly flag (as much as possible in this library)
      String cookieHeader = "auth=" + authCookie + "; Path=/; Max-Age=3600; SameSite=Strict";
      server.sendHeader("Set-Cookie", cookieHeader);
      server.sendHeader("Location", "/");
      server.send(303);

      Serial.println("User " + username + " logged in successfully, set authentication cookie.");
    } else {
      // Redirect back to login with error
      server.sendHeader("Location", "/login?error=1");
      server.send(303);

      Serial.println("Failed login attempt - username: " + username);
    }
  });

  // Logout handler
  server.on("/logout", HTTP_GET, [&]() {
    // Clear the auth cookie
    server.sendHeader("Set-Cookie", "auth=; Path=/; Max-Age=0");
    server.sendHeader("Location", "/login");
    server.send(303);

    Serial.println("User logged out");
  });

  // Root page - configuration interface
  server.on("/", HTTP_GET, [&]() {
    // Check authentication
    if (!is_authenticated()) {
      redirect_to_login();
      return;
    }

    // Send response headers first
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    // Process HTML from PROGMEM in chunks to avoid RAM issues
    const size_t CHUNK_SIZE = 768;
    const size_t OVERLAP = 50; // Overlap to handle placeholders split across chunks
    char buffer[CHUNK_SIZE + OVERLAP + 1];
    size_t len = strlen_P(webpage_dashboard);
    size_t pos = 0;
    String carryOver = "";

    while (pos < len) {
      size_t chunk = min(CHUNK_SIZE, len - pos);
      memcpy_P(buffer, webpage_dashboard + pos, chunk);
      buffer[chunk] = '\0';

      String htmlChunk = carryOver + String(buffer);
      carryOver = "";

      // Replace placeholders
      htmlChunk.replace("{HOSTNAME}", runtime.deviceHostname);
      htmlChunk.replace("{MAC_ADDRESS}", runtime.get_ethernet_mac_address());
      htmlChunk.replace("{IP_ADDRESS}", runtime.ethernetIP);
      if (runtime.lldp.hasValidLLDPData()) {
        htmlChunk.replace("{LLDP_NEIGHBOR}", runtime.lldp.getSwitchHostname() + " - " + runtime.lldp.getSwitchPortId());
      } else {
        htmlChunk.replace("{LLDP_NEIGHBOR}", "No LLDP neighbor detected");
      }
      htmlChunk.replace("{LED_PATTERN}", String(runtime.ledManager.runningPattern));
      htmlChunk.replace("{RELAY_PATTERN_1}", String(runtime.relay1.relayState));
      htmlChunk.replace("{RELAY_PATTERN_2}", String(runtime.relay2.relayState));
      htmlChunk.replace("{SOFTWARE_VERSION}", SOFTWARE_VERSION);

      if (runtime.sipLine1.is_registered() && runtime.sipLine1.is_ringing()) {
        htmlChunk.replace("{LINE_1_STATUS}", "ringing");
      } else if (runtime.sipLine1.is_registered()) {
        htmlChunk.replace("{LINE_1_STATUS}", "registered");
      } else if (runtime.sipLine1.is_configured()) {
        htmlChunk.replace("{LINE_1_STATUS}", "configured");
      } else {
        htmlChunk.replace("{LINE_1_STATUS}", "notconfigured");
      }

      if (runtime.sipLine2.is_registered() && runtime.sipLine1.is_ringing()) {
        htmlChunk.replace("{LINE_2_STATUS}", "ringing");
      } else if (runtime.sipLine2.is_registered()) {
        htmlChunk.replace("{LINE_2_STATUS}", "registered");
      } else if (runtime.sipLine2.is_configured()) {
        htmlChunk.replace("{LINE_2_STATUS}", "configured");
      } else {
        htmlChunk.replace("{LINE_2_STATUS}", "notconfigured");
      }

      htmlChunk.replace("{SIP_SERVER_1}", runtime.sipLine1.sipServer);
      htmlChunk.replace("{SIP_PORT_1}", String(runtime.sipLine1.sipPort));
      htmlChunk.replace("{SIP_USERNAME_1}", runtime.sipLine1.sipUsername);
      htmlChunk.replace("{SIP_PASSWORD_1}", runtime.sipLine1.sipPassword);
      htmlChunk.replace("{SIP_SERVER_2}", runtime.sipLine2.sipServer);
      htmlChunk.replace("{SIP_PORT_2}", String(runtime.sipLine2.sipPort));
      htmlChunk.replace("{SIP_USERNAME_2}", runtime.sipLine2.sipUsername);
      htmlChunk.replace("{SIP_PASSWORD_2}", runtime.sipLine2.sipPassword);

      htmlChunk.replace("{LED_IDLE}", String(runtime.idlePattern));
      htmlChunk.replace("{LED_RING_1}", String(runtime.line1RingPattern));
      htmlChunk.replace("{LED_RING_2}", String(runtime.line2RingPattern));
      htmlChunk.replace("{LED_ERROR_1}", String(runtime.line1ErrorPattern));
      htmlChunk.replace("{LED_ERROR_2}", String(runtime.line2ErrorPattern));

      htmlChunk.replace("{RELAY_1}", String(runtime.relay1Config));
      htmlChunk.replace("{RELAY_2}", String(runtime.relay2Config));

      // If not the last chunk, save the last OVERLAP characters for next iteration
      if (pos + chunk < len) {
        size_t sendLen = htmlChunk.length() - OVERLAP;
        server.sendContent(htmlChunk.substring(0, sendLen));
        carryOver = htmlChunk.substring(sendLen);
      } else {
        server.sendContent(htmlChunk);
      }

      pos += chunk;
    }

    server.sendContent(""); // Signal end of content
  });

  // Save SIP configuration
  server.on("/save-sip", HTTP_POST, [&]() {
    // Check authentication
    if (!is_authenticated()) {
      redirect_to_login();
      return;
    }

    if (server.hasArg("sip_server_1") && server.hasArg("sip_port_1") && 
      server.hasArg("sip_username_1") && server.hasArg("sip_password_1")) {
        runtime.sipLine1.update_credentials(server.arg("sip_server_1"), server.arg("sip_port_1").toInt(),
        server.arg("sip_username_1"), server.arg("sip_password_1"), server.arg("sip_server_1"));
    }

    if (server.hasArg("sip_server_2") && server.hasArg("sip_port_2") && 
      server.hasArg("sip_username_2") && server.hasArg("sip_password_2")) {
        runtime.sipLine2.update_credentials(server.arg("sip_server_2"), server.arg("sip_port_2").toInt(),
        server.arg("sip_username_2"), server.arg("sip_password_2"), server.arg("sip_server_2"));
    }

    runtime.save_configuration();

    runtime.sipLine1.begin_registration();
    runtime.sipLine2.begin_registration();

    server.sendHeader("Location", "/?save=sip");
    server.send(303);
  });

  // Save behavior configuration
  server.on("/save-behavior", HTTP_POST, [&]() {
    // Check authentication
    if (!is_authenticated()) {
      redirect_to_login();
      return;
    }

    if (server.hasArg("led_idle")) runtime.idlePattern = server.arg("led_idle").toInt();
    if (server.hasArg("led_ring_1")) runtime.line1RingPattern = server.arg("led_ring_1").toInt();
    if (server.hasArg("led_ring_2")) runtime.line2RingPattern = server.arg("led_ring_2").toInt();
    if (server.hasArg("led_error_1")) runtime.line1ErrorPattern = server.arg("led_error_1").toInt();
    if (server.hasArg("led_error_2")) runtime.line2ErrorPattern = server.arg("led_error_2").toInt();

    if (server.hasArg("relay_1")) runtime.relay1Config = server.arg("relay_1").toInt();
    if (server.hasArg("relay_2")) runtime.relay2Config = server.arg("relay_2").toInt();

    runtime.save_configuration();

    server.sendHeader("Location", "/?save=behavior");
    server.send(303);
  });

  // Save Device configuration
  server.on("/save-device", HTTP_POST, [&]() {
    // Check authentication
    if (!is_authenticated()) {
      redirect_to_login();
      return;
    }

    if (server.hasArg("hostname")) runtime.deviceHostname = server.arg("hostname");
    if (server.hasArg("admin_password")) runtime.webPassword = server.arg("admin_password");

    runtime.save_configuration();

    server.sendHeader("Location", "/?save=device");
    server.send(303);
  });

  // Manual SIP registration
  server.on("/register-now", HTTP_POST, [&]() {
    // Check authentication
    if (!is_authenticated()) {
      redirect_to_login();
      return;
    }

    runtime.sipLine1.begin_registration();
    runtime.sipLine2.begin_registration();
    
    server.sendHeader("Location", "/?save=register-now");
    server.send(303);
  });

  server.begin();
  Serial.println("Web server started on port 80");
  Serial.print("Server should be accessible at: http://");
  Serial.println(ETH.localIP());
}

void ConfigServer::handle() {
  server.handleClient();
}