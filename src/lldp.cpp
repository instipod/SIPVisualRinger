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

void LLDPService::init() {
    if (eth_handle == NULL) {
        eth_handle = getEthHandle();
        if (eth_handle == NULL) {
            Serial.println("Warning: Could not get Ethernet handle for LLDP");
            Serial.println("LLDP will not be available");
        } else {
            Serial.println("Ethernet handle obtained successfully for LLDP");

            // Register callback to receive LLDP frames
            esp_err_t err = esp_eth_update_input_path(eth_handle, lldpFrameReceiver, this);
            if (err == ESP_OK) {
                Serial.println("LLDP frame receiver registered successfully");
            } else {
                Serial.println("Failed to register LLDP frame receiver: " + String(err));
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
    // Check if this is an LLDP frame (EtherType 0x88cc)
    if (len >= 14 && buffer[12] == 0x88 && buffer[13] == 0xcc) {
        LLDPService *service = (LLDPService *)priv;
        service->parseLLDPFrame(buffer, len);
    }

    // Return ESP_OK to continue normal packet processing
    return ESP_OK;
}

// Parse received LLDP frame and extract switch information
void LLDPService::parseLLDPFrame(uint8_t *frame, uint16_t length) {
    Serial.println("Received LLDP frame (" + String(length) + " bytes)");

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
                Serial.print("Chassis ID: ");
                if (tlvLength > 1) {
                    uint8_t subtype = frame[pos];
                    Serial.print("Subtype=" + String(subtype) + " ");
                    // Print the value (skip subtype byte)
                    for (uint16_t i = 1; i < tlvLength; i++) {
                        Serial.print(String(frame[pos + i], HEX) + " ");
                    }
                }
                Serial.println();
                break;

            case 2: // Port ID
                if (tlvLength > 1) {
                    uint8_t subtype = frame[pos];
                    // Extract port ID string (skip subtype byte)
                    tempPortId = "";
                    for (uint16_t i = 1; i < tlvLength; i++) {
                        tempPortId += (char)frame[pos + i];
                    }
                    Serial.println("Port ID: " + tempPortId + " (subtype=" + String(subtype) + ")");
                }
                break;

            case 3: // TTL
                if (tlvLength == 2) {
                    uint16_t ttl = (frame[pos] << 8) | frame[pos + 1];
                    Serial.println("TTL: " + String(ttl) + " seconds");
                }
                break;

            case 4: // Port Description
                tempPortDesc = "";
                for (uint16_t i = 0; i < tlvLength; i++) {
                    tempPortDesc += (char)frame[pos + i];
                }
                Serial.println("Port Description: " + tempPortDesc);
                break;

            case 5: // System Name (this is the switch hostname!)
                tempHostname = "";
                for (uint16_t i = 0; i < tlvLength; i++) {
                    tempHostname += (char)frame[pos + i];
                }
                Serial.println("System Name (Switch Hostname): " + tempHostname);
                break;

            case 6: // System Description
                {
                    String sysDesc = "";
                    for (uint16_t i = 0; i < tlvLength; i++) {
                        sysDesc += (char)frame[pos + i];
                    }
                    Serial.println("System Description: " + sysDesc);
                }
                break;

            case 7: // System Capabilities
                if (tlvLength == 4) {
                    uint16_t capabilities = (frame[pos] << 8) | frame[pos + 1];
                    uint16_t enabled = (frame[pos + 2] << 8) | frame[pos + 3];
                    Serial.println("Capabilities: 0x" + String(capabilities, HEX) + " Enabled: 0x" + String(enabled, HEX));
                }
                break;

            case 8: // Management Address
                Serial.println("Management Address TLV received");
                break;

            default:
                Serial.println("Unknown TLV Type: " + String(tlvType) + " Length: " + String(tlvLength));
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

        Serial.println("=== LLDP Data Updated ===");
        Serial.println("Switch Hostname: " + switchHostname);
        Serial.println("Switch Port ID: " + switchPortId);
        Serial.println("Switch Port Desc: " + switchPortDesc);
        Serial.println("========================");
    }
}