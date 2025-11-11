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
    String html = String(webpage_login);
    html.replace("{HOSTNAME}", runtime.deviceHostname);
    html.replace("{MAC_ADDRESS}", runtime.get_ethernet_mac_address());
    server.send(200, "text/html", html);
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

    String html = String(webpage_dashboard);
    html.replace("{HOSTNAME}", runtime.deviceHostname);
    html.replace("{MAC_ADDRESS}", runtime.get_ethernet_mac_address());
    html.replace("{IP_ADDRESS}", runtime.ethernetIP);
    if (runtime.lldp.hasValidLLDPData()) {
      html.replace("{LLDP_NEIGHBOR}", runtime.lldp.getSwitchHostname() + " - " + runtime.lldp.getSwitchPortId());
    } else {
      html.replace("{LLDP_NEIGHBOR}", "No LLDP neighbor detected");
    }
    html.replace("{SIP_SERVER_1}", runtime.sipLine1.sipServer);
    html.replace("{SIP_PORT_1}", String(runtime.sipLine1.sipPort));
    html.replace("{SIP_USERNAME_1}", runtime.sipLine1.sipUsername);
    html.replace("{SIP_PASSWORD_1}", runtime.sipLine1.sipPassword);
    html.replace("{SIP_SERVER_2}", runtime.sipLine2.sipServer);
    html.replace("{SIP_PORT_2}", String(runtime.sipLine2.sipPort));
    html.replace("{SIP_USERNAME_2}", runtime.sipLine2.sipUsername);
    html.replace("{SIP_PASSWORD_2}", runtime.sipLine2.sipPassword);
    html.replace("{LED_PATTERN}", String(runtime.ledManager.runningPattern));

    server.send(200, "text/html", html);
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

    if (server.hasArg("hostname")) runtime.deviceHostname = server.arg("hostname");
    if (server.hasArg("admin_password")) runtime.webPassword = server.arg("admin_password");

    runtime.save_configuration();

    server.sendHeader("Location", "/?save=behavior");
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