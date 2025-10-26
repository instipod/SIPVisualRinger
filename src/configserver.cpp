#include "configserver.h"

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

  // Login page
  server.on("/login", HTTP_GET, [&]() {
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
    html += "<p><strong>IP Address:</strong> " + runtime.ethernetIP + "</p>";
    html += "<p><strong>MAC Address:</strong> " + runtime.get_ethernet_mac_address() + "</p>";
    html += "<p><strong>Hostname:</strong> " + runtime.deviceHostname + "</p>";
    html += "<p><strong>Link Status:</strong> " + String(ETH.linkUp() ? "Up" : "Down") + "</p>";
    html += "<p><strong>SIP 1 Status:</strong> " + String(runtime.sipLine1.is_registered() ? "Registered" : "Not Registered") + "</p>";
    html += "<p><strong>SIP 2 Status:</strong> " + String(runtime.sipLine2.is_registered() ? "Registered" : "Not Registered") + "</p>";

    // LED status with description
    String ledStatus;
    switch (runtime.currentLedMode) {
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
    html += "<p><label>Hostname:</label><input type='text' name='hostname' value='" + runtime.deviceHostname + "'></p>";
    html += "<p><label>SIP Server:</label><input type='text' name='sipServer' value='" + runtime.sipLine1.sipServer + "'></p>";
    html += "<p><label>SIP Port:</label><input type='number' name='sipPort' value='" + String(runtime.sipLine1.sipPort) + "'></p>";
    html += "<p><label>SIP Username:</label><input type='text' name='sipUsername' value='" + runtime.sipLine1.sipUsername + "'></p>";
    html += "<p><label>SIP Password:</label><input type='password' name='sipPassword' value='" + runtime.sipLine1.sipPassword + "'></p>";
    html += "<p><label>SIP Realm:</label><input type='text' name='sipRealm' value='" + runtime.sipLine1.sipRealm + "'></p>";
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
  server.on("/save", HTTP_POST, [&]() {
    // Check authentication
    if (!is_authenticated()) {
      redirect_to_login();
      return;
    }

    if (server.hasArg("hostname")) runtime.deviceHostname = server.arg("hostname");
    if (server.hasArg("sipServer") && server.hasArg("sipPort") && server.hasArg("sipUsername") && server.hasArg("sipPassword")
      && server.hasArg("sipRealm")) {
        runtime.sipLine1.update_credentials(server.arg("sipServer"), server.arg("sipPort").toInt(),
        server.arg("sipUsername"), server.arg("sipPassword"), server.arg("sipRealm"));
    }

    runtime.save_configuration();

    server.sendHeader("Location", "/");
    server.send(303);
  });

  // Manual SIP registration
  server.on("/register", HTTP_POST, [&]() {
    // Check authentication
    if (!is_authenticated()) {
      redirect_to_login();
      return;
    }

    runtime.sipLine1.begin_registration();
    runtime.sipLine2.begin_registration();
    
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.begin();
  Serial.println("Web server started on port 80");
}

void ConfigServer::handle() {
  server.handleClient();
}