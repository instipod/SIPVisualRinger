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
        esp_netif_t *netif = NULL;
        unsigned long lastLLDPTime = 0;
        unsigned long lldpInterval = LLDP_INTERVAL;

        String &hostname;
        String description;

        // Received LLDP information from switch
        String switchHostname = "";
        String switchPortId = "";
        String switchPortDesc = "";
        unsigned long lastLLDPReceived = 0;
        bool lldpDataValid = false;

        esp_eth_handle_t getEthHandle();
        void parseLLDPFrame(uint8_t *frame, uint16_t length);
        static esp_err_t lldpFrameReceiver(esp_eth_handle_t hdl, uint8_t *buffer, uint32_t len, void *priv);
    public:
        bool enabled = true;

        LLDPService(String &h, String d) : hostname(h), description(d) {}

        void init();
        void update_description(String description);
        void handle();
        void send();

        // Methods to retrieve switch information
        String getSwitchHostname() { return switchHostname; }
        String getSwitchPortId() { return switchPortId; }
        String getSwitchPortDesc() { return switchPortDesc; }
        bool hasValidLLDPData() { return lldpDataValid && (millis() - lastLLDPReceived < 180000); } // Valid for 3 minutes
};
#endif