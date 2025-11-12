#ifndef RUNTIME_H
#define RUNTIME_H
#include <Arduino.h>
#include <sip.h>
#include <configstore.h>
#include <lldp.h>
#include <ESPmDNS.h>
#include <led-manager.h>
#include <relay-manager.h>
#include "esp_mac.h"
extern "C" {
#include "bootloader_random.h"
}

#define RELAY1 6
#define RELAY2 5

class Runtime {
    public:
        ConfigStore configStore;

        String deviceHostname = "VisualAlert-FFFFF";
        String ethernetIP = "0.0.0.0";

        int idlePattern = GREEN_SOLID;
        int line1RingPattern = RED_CHASE;
        int line2RingPattern = BLUE_CHASE;
        int line1ErrorPattern = RED_SOLID;
        int line2ErrorPattern = RED_SOLID;
        LedManager ledManager;

        int relay1Config = ON_WHILE_LINE1;
        int relay2Config = ON_WHILE_LINE2;
        RelayManager relay1 = RelayManager(RELAY1);
        RelayManager relay2 = RelayManager(RELAY2);

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