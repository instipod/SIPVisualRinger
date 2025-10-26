#include <lldp.h>

esp_eth_handle_t LLDPService::getEthHandle() {
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
  if (netif == NULL) {
    Serial.println("Could not get netif handle");
    return NULL;
  }
  
  // Get the IO driver (returns void* which is the esp_eth_handle_t)
  void *driver = esp_netif_get_io_driver(netif);
  if (driver == NULL) {
    Serial.println("Could not get IO driver");
    return NULL;
  }
  
  // The driver is the esp_eth_handle_t
  return (esp_eth_handle_t)driver;
}

void LLDPService::init(String &hostname, String description) {
    if (eth_handle == NULL) {
        eth_handle = getEthHandle();
        if (eth_handle == NULL) {
            Serial.println("Warning: Could not get Ethernet handle for LLDP");
            Serial.println("LLDP will not be available");
        } else {
            Serial.println("Ethernet handle obtained successfully for LLDP");
        }
    }

    this->hostname = hostname;
    this->description = description;
}

void LLDPService::handle() {
    if (!enabled) {
        return;
    }
    if ((millis() - lastLLDPTime) > lldpInterval) {
        this->send();
        lastLLDPTime = millis();
    }
}

void LLDPService::send() {
    if (!enabled) {
        return;
    }
    if (!ETH.linkUp() || eth_handle == NULL) {
        return;
    }

    Serial.println("Sending LLDP frame...");

    // Prepare LLDP frame buffer (max 1500 bytes)
    uint8_t lldpFrame[512];
    int framePos = 0;

    // Ethernet Header
    // Destination MAC: LLDP multicast address 01:80:c2:00:00:0e
    lldpFrame[framePos++] = 0x01;
    lldpFrame[framePos++] = 0x80;
    lldpFrame[framePos++] = 0xc2;
    lldpFrame[framePos++] = 0x00;
    lldpFrame[framePos++] = 0x00;
    lldpFrame[framePos++] = 0x0e;

    // Source MAC: Our MAC address
    uint8_t mac[6];
    sscanf(ETH.macAddress().c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
            &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    for (int i = 0; i < 6; i++) {
        lldpFrame[framePos++] = mac[i];
    }

    // EtherType: LLDP (0x88cc)
    lldpFrame[framePos++] = 0x88;
    lldpFrame[framePos++] = 0xcc;

    // LLDP Payload starts here

    // TLV 1: Chassis ID (Type=1)
    // Format: Type(7 bits) | Length(9 bits) | Subtype(1 byte) | Value
    lldpFrame[framePos++] = 0x02;  // Type=1, Length=7 (high 7 bits of 0x02 = type 1)
    lldpFrame[framePos++] = 0x07;  // Length = 7 bytes (1 subtype + 6 MAC)
    lldpFrame[framePos++] = 0x04;  // Subtype: MAC address (4)
    // MAC address
    for (int i = 0; i < 6; i++) {
        lldpFrame[framePos++] = mac[i];
    }

    // TLV 2: Port ID (Type=2)
    String portID = "Ethernet";
    uint8_t portIDLen = portID.length() + 1; // +1 for subtype
    lldpFrame[framePos++] = 0x04;  // Type=2, length high bits
    lldpFrame[framePos++] = portIDLen;  // Length
    lldpFrame[framePos++] = 0x05;  // Subtype: Interface name (5)
    for (unsigned int i = 0; i < portID.length(); i++) {
        lldpFrame[framePos++] = portID[i];
    }

    // TLV 3: TTL (Type=3)
    lldpFrame[framePos++] = 0x06;  // Type=3, length high bits
    lldpFrame[framePos++] = 0x02;  // Length = 2 bytes
    lldpFrame[framePos++] = 0x00;  // TTL high byte (120 seconds)
    lldpFrame[framePos++] = 0x78;  // TTL low byte (120 seconds)

    // TLV 5: System Name (Type=5)
    uint8_t hostnameLen = hostname.length();
    lldpFrame[framePos++] = 0x0a;  // Type=5, length high bits
    lldpFrame[framePos++] = hostnameLen;  // Length
    for (unsigned int i = 0; i < hostname.length(); i++) {
        lldpFrame[framePos++] = hostname[i];
    }

    // TLV 6: System Description (Type=6) - Optional
    uint8_t sysDescLen = description.length();
    lldpFrame[framePos++] = 0x0c;  // Type=6, length high bits
    lldpFrame[framePos++] = sysDescLen;  // Length
    for (unsigned int i = 0; i < description.length(); i++) {
        lldpFrame[framePos++] = description[i];
    }

    // TLV 8: Management Address (Type=8)
    IPAddress localIP = ETH.localIP();
    // Address String Length (1) + Address Subtype (1) + Address (4) + 
    // Interface Subtype (1) + Interface Number (4) + OID Length (1)
    lldpFrame[framePos++] = 0x10;  // Type=8, length high bits
    lldpFrame[framePos++] = 0x0c;  // Length = 12 bytes
    lldpFrame[framePos++] = 0x05;  // Address string length = 5 (1 subtype + 4 IP)
    lldpFrame[framePos++] = 0x01;  // Address subtype: IPv4 (1)
    lldpFrame[framePos++] = localIP[0];  // IP address byte 1
    lldpFrame[framePos++] = localIP[1];  // IP address byte 2
    lldpFrame[framePos++] = localIP[2];  // IP address byte 3
    lldpFrame[framePos++] = localIP[3];  // IP address byte 4
    lldpFrame[framePos++] = 0x01;  // Interface numbering subtype: ifIndex (1)
    lldpFrame[framePos++] = 0x00;  // Interface number (4 bytes) = 1
    lldpFrame[framePos++] = 0x00;
    lldpFrame[framePos++] = 0x00;
    lldpFrame[framePos++] = 0x01;
    lldpFrame[framePos++] = 0x00;  // OID string length = 0 (no OID)

    // TLV 0: End of LLDPDU (Type=0, Length=0)
    lldpFrame[framePos++] = 0x00;
    lldpFrame[framePos++] = 0x00;

    // Send the raw Ethernet frame
    esp_err_t err = esp_eth_transmit(eth_handle, lldpFrame, framePos);

    if (err == ESP_OK) {
        Serial.println("LLDP frame sent successfully (" + String(framePos) + " bytes)");
    } else {
        Serial.println("LLDP frame send failed: " + String(err));
    }
}