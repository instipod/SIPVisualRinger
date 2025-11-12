#include <runtime.h>
#include "esp_random.h"

void Runtime::load_configuration() {
    String ethernetMAC = get_ethernet_mac_address();
    ethernetMAC.replace(":", "");

    deviceHostname = configStore.get_string("hostname", "VisualAlert-" + ethernetMAC.substring(ethernetMAC.length() - 5, ethernetMAC.length()));

    sipLine1.update_credentials(configStore.get_string("sipServer1"), configStore.get_integer("sipPort1"),
        configStore.get_string("sipUsername1"), configStore.get_string("sipPassword1"), configStore.get_string("sipRealm1"));
    sipLine2.update_credentials(configStore.get_string("sipServer2"), configStore.get_integer("sipPort2"),
        configStore.get_string("sipUsername2"), configStore.get_string("sipPassword2"), configStore.get_string("sipRealm2"));

    mDNSEnabled = configStore.get_boolean("mdnsEnabled", true);
    lldp.enabled = configStore.get_boolean("lldpEnabled", true);

    webPassword = configStore.get_string("webPassword", "admin");

    // Load LED pattern configurations
    idlePattern = configStore.get_integer("idlePattern", GREEN_SOLID);
    line1RingPattern = configStore.get_integer("line1RingPattern", YELLOW_FLASH);
    line2RingPattern = configStore.get_integer("line2RingPattern", BLUE_FLASH);
    line1ErrorPattern = configStore.get_integer("line1ErrorPattern", RED_SOLID);
    line2ErrorPattern = configStore.get_integer("line2ErrorPattern", RED_SOLID);

    // Load relay configurations
    relay1Config = configStore.get_integer("relay1Config", ON_WHILE_LINE1);
    relay2Config = configStore.get_integer("relay2Config", ON_WHILE_LINE2);
}

void Runtime::save_configuration() {
    configStore.put_string("hostname", deviceHostname);

    configStore.put_string("sipServer1", sipLine1.sipServer);
    configStore.put_integer("sipPort1", sipLine1.sipPort);
    configStore.put_string("sipUsername1", sipLine1.sipUsername);
    configStore.put_string("sipPassword1", sipLine1.sipPassword);
    configStore.put_string("sipRealm1", sipLine1.sipRealm);

    configStore.put_string("sipServer2", sipLine2.sipServer);
    configStore.put_integer("sipPort2", sipLine2.sipPort);
    configStore.put_string("sipUsername2", sipLine2.sipUsername);
    configStore.put_string("sipPassword2", sipLine2.sipPassword);
    configStore.put_string("sipRealm2", sipLine2.sipRealm);

    configStore.put_boolean("lldpEnabled", lldp.enabled);
    configStore.put_boolean("mdnsEnabled", mDNSEnabled);

    configStore.put_string("webPassword", webPassword);

    // Save LED pattern configurations
    configStore.put_integer("idlePattern", idlePattern);
    configStore.put_integer("line1RingPattern", line1RingPattern);
    configStore.put_integer("line2RingPattern", line2RingPattern);
    configStore.put_integer("line1ErrorPattern", line1ErrorPattern);
    configStore.put_integer("line2ErrorPattern", line2ErrorPattern);

    // Save relay configurations
    configStore.put_integer("relay1Config", relay1Config);
    configStore.put_integer("relay2Config", relay2Config);

    ETH.setHostname(deviceHostname.c_str());
}

void Runtime::init() {
    // Enable the internal voltage reference as random seed
    // Disables WiFi and BLE
    bootloader_random_enable();
    Serial.println("Cryptographic RNG initialized with ESP32 hardware RNG");

    configStore.init();
    Serial.println("ConfigStore initialized.");

    ledManager.init();
    Serial.println("LED Manager initialized.");

    relay1.init();
    relay2.init();
    Serial.println("Relay Manager initialized.");
}

void Runtime::handle() {
    sipLine1.handle();
    sipLine2.handle();

    lldp.handle();

    ledManager.handle();

    relay1.handle();
    relay2.handle();
}

void Runtime::ip_begin() {
    sipLine1.init();
    sipLine2.init();

    if (mDNSEnabled) {
        MDNS.end();
        MDNS.disableArduino();
        MDNS.begin(deviceHostname.c_str());
        MDNS.addService("http", "_tcp", 80);
        MDNS.addService("visualalert", "_tcp", 80);
    }

    if (sipLine1.is_configured() && !sipLine1.is_registered()) {
        sipLine1.begin_registration();
    }
    if (sipLine2.is_configured() && !sipLine2.is_registered()) {
        sipLine2.begin_registration();
    }
}

void Runtime::ip_end() {
    sipLine1.end();
    sipLine2.end();

    MDNS.end();
}

int Runtime::get_srandom_byte() {
    return esp_random();
}

String Runtime::get_random_string(int length) {
    String result = "";
    result.reserve(length);

    // Character set: alphanumeric (a-z, A-Z, 0-9)
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const int charset_size = sizeof(charset) - 1;

    for (int i = 0; i < length; i++) {
        unsigned char random_byte = get_srandom_byte();
        result += charset[random_byte % charset_size];
    }

    return result;
}

int Runtime::split_string(const String& data, char separator, String result[], int maxTokens) {
    int tokenCount = 0;
    int fromIndex = 0;
    int separatorIndex = -1;
    int dataLength = data.length();

    // Loop until we either run out of space in the result array or we hit the end of the string
    while (tokenCount < maxTokens) {
        // Find the next separator, starting the search from the end of the last token (fromIndex)
        separatorIndex = data.indexOf(separator, fromIndex);

        // Case 1: Separator found
        if (separatorIndex != -1) {
            // Extract the substring (the token) from the previous position up to the separator
            result[tokenCount++] = data.substring(fromIndex, separatorIndex);
            
            // Update the start position for the next search to be after the separator
            fromIndex = separatorIndex + 1;
        } 
        
        // Case 2: No more separator found or we are at the end of the string
        else {
            // Check if there is any remaining text to capture (the final token)
            if (fromIndex <= dataLength) {
                // Take the rest of the string
                result[tokenCount++] = data.substring(fromIndex);
            }
            // Stop processing after the last token is captured
            break;
        }
    }
    
    return tokenCount;
}

void Runtime::get_ethernet_mac(uint8_t baseMac[6]) {
   // Get MAC address for WiFi station, but we are using it for ethernet
   esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
}

String Runtime::get_ethernet_mac_address() {
   uint8_t baseMac[6];
   get_ethernet_mac(baseMac);

   char baseMacChr[18] = {0};
   sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
   return String(baseMacChr);
}