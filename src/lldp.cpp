#include <lldp.h>
#include <ETH.h>

// W5500 register definitions for raw socket mode
#define SnMR_MACRAW 0x04  // MAC RAW mode
#define Sn_MR 0x0000      // Socket Mode Register

esp_eth_handle_t LLDPService::getEthHandle() {
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
  if (netif == NULL) {
    Serial.println("Could not get netif handle");
    return NULL;
  }

  // Store netif for later use in packet forwarding
  this->netif = netif;

  // Get the IO driver (returns void* which is the esp_eth_handle_t)
  void *driver = esp_netif_get_io_driver(netif);
  if (driver == NULL) {
    Serial.println("Could not get IO driver");
    return NULL;
  }

  // The driver is the esp_eth_handle_t
  return (esp_eth_handle_t)driver;
}

void LLDPService::init() {
    if (eth_handle == NULL) {
        eth_handle = getEthHandle();
        if (eth_handle == NULL) {
            Serial.println("ERROR: Could not get Ethernet handle for LLDP");
            Serial.println("LLDP will not be available");
        } else {
            // Enable promiscuous mode to receive all frames including multicast
            Serial.println("Enabling promiscuous mode...");
            bool promiscuous = true;
            esp_err_t promisc_err = esp_eth_ioctl(eth_handle, ETH_CMD_S_PROMISCUOUS, &promiscuous);
            if (promisc_err != ESP_OK) {
                Serial.print("WARNING: Failed to enable promiscuous mode: 0x");
                Serial.println(promisc_err, HEX);
                Serial.println("Trying alternate approach - enabling receive all multicast...");

                // Try ETH_CMD_S_MULTICAST_ALL as fallback
                bool rx_allmulti = true;
                esp_err_t multicast_err = esp_eth_ioctl(eth_handle, (esp_eth_io_cmd_t)0x8003, &rx_allmulti);  // ETH_CMD_S_RX_ALLMULTI
                if (multicast_err == ESP_OK) {
                    Serial.println("SUCCESS: Receive all multicast enabled");
                } else {
                    Serial.print("WARNING: Failed to enable multicast reception: 0x");
                    Serial.println(multicast_err, HEX);
                }
            }

            esp_err_t err = esp_eth_update_input_path(eth_handle, lldpFrameReceiver, this);
            if (err != ESP_OK) {
                Serial.print("ERROR: Failed to register LLDP frame receiver: 0x");
                Serial.println(err, HEX);
            }
        }
    }
}

void LLDPService::update_description(String description) {
    this->description = description;
}

void LLDPService::handle() {
    if (!enabled) {
        return;
    }
    if ((millis() - lastLLDPTime) > lldpInterval || lastLLDPTime == 0) {
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
    String portID = "1";
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

    // TLV 4: Port Description (Type=4)
    String portDesc = "Ethernet Port";
    uint8_t portDescLen = portDesc.length();
    lldpFrame[framePos++] = 0x08;  // Type=4, length high bits
    lldpFrame[framePos++] = portDescLen;  // Length
    for (unsigned int i = 0; i < portDesc.length(); i++) {
        lldpFrame[framePos++] = portDesc[i];
    }

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

    // TLV 7: System Capabilities (Type=7)
    // Length = 4 bytes (2 bytes capabilities + 2 bytes enabled)
    lldpFrame[framePos++] = 0x0e;  // Type=7, length high bits
    lldpFrame[framePos++] = 0x04;  // Length = 4 bytes
    // System Capabilities (2 bytes, bit 7 = Station Only / End Device)
    lldpFrame[framePos++] = 0x00;  // High byte
    lldpFrame[framePos++] = 0x80;  // Low byte (bit 7 set = Station Only)
    // Enabled Capabilities (2 bytes, bit 7 = Station Only / End Device)
    lldpFrame[framePos++] = 0x00;  // High byte
    lldpFrame[framePos++] = 0x80;  // Low byte (bit 7 set = Station Only)

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

// Static callback function for receiving Ethernet frames
esp_err_t LLDPService::lldpFrameReceiver(esp_eth_handle_t hdl, uint8_t *buffer, uint32_t len, void *priv) {
    LLDPService *service = (LLDPService *)priv;

    // Check for LLDP multicast destination MAC: 01:80:c2:00:00:0e
    bool isLldpDest = (len >= 6 &&
                       buffer[0] == 0x01 &&
                       buffer[1] == 0x80 &&
                       buffer[2] == 0xc2 &&
                       buffer[3] == 0x00 &&
                       buffer[4] == 0x00 &&
                       buffer[5] == 0x0e);

    // Check if this is an LLDP frame (EtherType 0x88cc at position 12-13)
    if (len >= 14 && buffer[12] == 0x88 && buffer[13] == 0xcc) {
        service->parseLLDPFrame(buffer, len);
        // Don't forward LLDP frames to the network stack
        return ESP_OK;
    }

    // Forward all non-LLDP packets to the network stack
    // This is critical because esp_eth_update_input_path replaces the default handler
    if (service->netif != NULL) {
        esp_err_t ret = esp_netif_receive(service->netif, buffer, len, NULL);
        if (ret != ESP_OK) {
            Serial.println("Failed to forward packet to netif: " + String(ret));
        }
        return ret;
    }

    return ESP_OK;
}

// Parse received LLDP frame and extract switch information
void LLDPService::parseLLDPFrame(uint8_t *frame, uint16_t length) {
    // LLDP payload starts after Ethernet header (14 bytes)
    uint16_t pos = 14;

    // Temporary storage for parsed data
    String tempHostname = "";
    String tempPortId = "";
    String tempPortDesc = "";

    // Parse TLVs
    while (pos < length - 2) {
        // Read TLV header (2 bytes)
        uint8_t typeAndLength1 = frame[pos++];
        uint8_t typeAndLength2 = frame[pos++];

        // Extract type (7 bits) and length (9 bits)
        uint8_t tlvType = typeAndLength1 >> 1;
        uint16_t tlvLength = ((typeAndLength1 & 0x01) << 8) | typeAndLength2;

        // Check if we have enough data
        if (pos + tlvLength > length) {
            Serial.println("Invalid LLDP TLV length, stopping parse");
            break;
        }

        // End of LLDPDU
        if (tlvType == 0 && tlvLength == 0) {
            break;
        }

        // Parse specific TLV types
        switch (tlvType) {
            case 1: // Chassis ID
                break;

            case 2: // Port ID
                if (tlvLength > 1) {
                    uint8_t subtype = frame[pos];
                    // Extract port ID string (skip subtype byte)
                    tempPortId = "";
                    for (uint16_t i = 1; i < tlvLength; i++) {
                        tempPortId += (char)frame[pos + i];
                    }
                }
                break;

            case 3: // TTL
                if (tlvLength == 2) {
                    uint16_t ttl = (frame[pos] << 8) | frame[pos + 1];
                }
                break;

            case 4: // Port Description
                tempPortDesc = "";
                for (uint16_t i = 0; i < tlvLength; i++) {
                    tempPortDesc += (char)frame[pos + i];
                }
                break;

            case 5: // System Name (this is the switch hostname!)
                tempHostname = "";
                for (uint16_t i = 0; i < tlvLength; i++) {
                    tempHostname += (char)frame[pos + i];
                }
                break;

            case 6: // System Description
                {
                    String sysDesc = "";
                    for (uint16_t i = 0; i < tlvLength; i++) {
                        sysDesc += (char)frame[pos + i];
                    }
                }
                break;

            case 7: // System Capabilities
                if (tlvLength == 4) {
                    uint16_t capabilities = (frame[pos] << 8) | frame[pos + 1];
                    uint16_t enabled = (frame[pos + 2] << 8) | frame[pos + 3];
                }
                break;

            case 8: // Management Address
                break;

            default:
                //Serial.println("Unknown TLV Type: " + String(tlvType) + " Length: " + String(tlvLength));
                break;
        }

        // Move to next TLV
        pos += tlvLength;
    }

    // Update stored values if we got a hostname
    if (tempHostname.length() > 0) {
        switchHostname = tempHostname;
        switchPortId = tempPortId;
        switchPortDesc = tempPortDesc;
        lastLLDPReceived = millis();
        lldpDataValid = true;

        Serial.print("Received LLDP neighbor info: hostname=");
        Serial.print(switchHostname);
        Serial.print(", port=");
        Serial.println(switchPortId);
    }
}