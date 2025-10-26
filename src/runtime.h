#ifndef RUNTIME_H
#define RUNTIME_H
#include <Arduino.h>
#include <sip.h>
#include <configstore.h>
#include <lldp.h>
#include <ESPmDNS.h>
#include <leds.h>
#include "esp_mac.h"
extern "C" {
#include "bootloader_random.h"
}

class Runtime {
    public:
        ConfigStore configStore;

        String deviceHostname = "VisualAlert-FFFFF";
        String ethernetIP = "0.0.0.0";

        int currentLedMode = LED_BOOTUP;

        bool mDNSEnabled = true;
        LLDPService lldp = LLDPService(deviceHostname, "ESP32 SIP Device");

        SIPClient sipLine1 = SIPClient(5060);
        SIPClient sipLine2 = SIPClient(5061);

        String webPassword = "admin";

        void init();
        void load_configuration();
        void save_configuration();
        void ip_begin();
        void ip_end();
        void handle();

        void get_ethernet_mac(uint8_t baseMac[6]);
        String get_ethernet_mac_address();

        int get_srandom_byte();
        String get_random_string(int length);
        int split_string(const String& data, char separator, String result[], int maxTokens);
};
#endif