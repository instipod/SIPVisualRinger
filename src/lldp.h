#ifndef LLDP_H
#define LLDP_H
#include <Arduino.h>
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include <ETH.h>

#define LLDP_INTERVAL 30000 // 30 seconds

class LLDPService {
    private:
        esp_eth_handle_t eth_handle = NULL;
        unsigned long lastLLDPTime = 0;
        unsigned long lldpInterval = LLDP_INTERVAL;

        String hostname = "esp32";
        String description = "ESP32 Device";

        esp_eth_handle_t getEthHandle();
    public:
        bool enabled = true;

        void init(String &hostname, String description);
        void handle();
        void send();
};
#endif