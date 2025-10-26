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

        String &hostname;
        String description;

        esp_eth_handle_t getEthHandle();
    public:
        bool enabled = true;

        LLDPService(String &h, String d) : hostname(h), description(d) {}

        void init();
        void update_description(String description);
        void handle();
        void send();
};
#endif