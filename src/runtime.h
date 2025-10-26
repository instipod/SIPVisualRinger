#ifndef RUNTIME_H
#define RUNTIME_H
#include <Arduino.h>
#include <sip.h>
#include <configstore.h>
#include <lldp.h>
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include <leds.h>
extern "C" {
#include "bootloader_random.h"
}

class Runtime {
    public:
        ConfigStore configStore;

        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context ctr_drbg;

        String deviceHostname = "VisualAlert-ffff";
        String ethernetIP = "0.0.0.0";
        String ethernetMAC = "ff:ff:ff:ff:ff:ff";

        int currentLedMode = LED_BOOTUP;

        bool mDNSEnabled = true;
        LLDPService lldp;

        SIPClient sipLine1 = SIPClient(5060);
        SIPClient sipLine2 = SIPClient(5061);

        String webPassword = "admin";

        void init();
        void load_configuration();
        void save_configuration();
        void handle();
};
#endif