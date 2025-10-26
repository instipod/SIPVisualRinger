#include <sip.h>

String SIPClient::generateCallID() {
    String localIP = ETH.localIP().toString();
    return String(random(100000000)) + "@" + localIP;
}

String SIPClient::generateTag() {
    return String(random(100000000));
}

String SIPClient::calculateMD5(String input) {
    MD5Builder md5;
    md5.begin();
    md5.add(input);
    md5.calculate();
    return md5.toString();
}

String SIPClient::extractParameter(String message, String startDelim, String endDelim) {
    int startIdx = message.indexOf(startDelim);
    if (startIdx == -1) return "";
    
    startIdx += startDelim.length();
    int endIdx = message.indexOf(endDelim, startIdx);
    if (endIdx == -1) {
        // If endDelim not found, take rest of string (may need trimming)
        String result = message.substring(startIdx);
        result.trim();
        return result;
    }
    
    String result = message.substring(startIdx, endIdx);
    result.trim();
    return result;
}

bool SIPClient::is_registered() {
    return sipRegistered;
}

bool SIPClient::is_ringing() {
    return currentCallID != "";
}

void SIPClient::end_registration(bool networkLost) {
    this->sipRegistered = false;
    this->currentCallID = "";
    this->currentFromTag = "";
    this->currentToTag = "";
    this->authAttempts = 0;
}

void SIPClient::update_credentials(String sipServer, int sipPort, String sipUsername, String sipPassword, String sipRealm) {
    if (this->is_registered()) {
        this->end_registration(false);
    }

    this->sipServer = sipServer;
    this->sipPort = sipPort;
    this->sipUsername = sipUsername;
    this->sipPassword = sipPassword;
    this->sipRealm = sipRealm;
}

void SIPClient::begin_registration() {
    if (sipServer.isEmpty() || sipUsername.isEmpty()) {
        return;
    }
    
    Serial.println("Registering to SIP server...");
    
    // Get current IP
    String localIP = ETH.localIP().toString();
    
    // Generate call ID and tag
    String callID = generateCallID();
    String fromTag = generateTag();
    String branch = "z9hG4bK" + generateTag();
    
    // Build REGISTER request (RFC 3261 compliant)
    String registerMsg = "REGISTER sip:" + sipServer + " SIP/2.0\r\n";
    registerMsg += "Via: SIP/2.0/UDP " + localIP + ":" + String(localSipPort) + ";branch=" + branch + ";rport\r\n";
    registerMsg += "Max-Forwards: 70\r\n";
    registerMsg += "From: <sip:" + sipUsername + "@" + sipServer + ">;tag=" + fromTag + "\r\n";
    registerMsg += "To: <sip:" + sipUsername + "@" + sipServer + ">\r\n";
    registerMsg += "Call-ID: " + callID + "\r\n";
    registerMsg += "CSeq: 1 REGISTER\r\n";
    registerMsg += "Contact: <sip:" + sipUsername + "@" + localIP + ":" + String(localSipPort) + ">\r\n";
    registerMsg += "Allow: INVITE, ACK, CANCEL, BYE, OPTIONS\r\n";
    registerMsg += "Expires: ";
    registerMsg += String(SIP_REGISTER_EXPIRES);
    registerMsg += "\r\n";
    registerMsg += "User-Agent: ";
    registerMsg += SIP_USER_AGENT;
    registerMsg += "\r\n";
    registerMsg += "Content-Length: 0\r\n";
    registerMsg += "\r\n";
    
    // Send REGISTER
    this->send_sip_message(sipServer.c_str(), sipPort, registerMsg);
    
    lastRegisterTime = millis();
    
    Serial.println("REGISTER sent:");
    Serial.println(registerMsg);
}

void SIPClient::send_sip_message(String remoteIP, int remotePort, String message) {
    udpSIP.beginPacket(remoteIP.c_str(), remotePort);
    udpSIP.write((const uint8_t*)message.c_str(), message.length());
    udpSIP.endPacket();
    Serial.println("SIP Message sent:");
    Serial.println(message);
}

void SIPClient::handle_auth_challenge(String message, String remoteIP, int remotePort) {
    // Extract nonce and realm from challenge
    String nonce = extractParameter(message, "nonce=\"", "\"");
    String realm = extractParameter(message, "realm=\"", "\"");
    String algorithm = extractParameter(message, "algorithm=", ",");
    if (algorithm.isEmpty()) {
        algorithm = extractParameter(message, "algorithm=", "\r");
    }
    
    // Clean up algorithm - remove quotes and whitespace
    algorithm.trim();
    algorithm.replace("\"", "");
    if (algorithm.isEmpty()) {
        algorithm = "MD5";  // Default to MD5
    }
    
    // Extract Call-ID, From tag, and CSeq from the 401 response
    String callID = extractParameter(message, "Call-ID: ", "\r\n");
    callID.trim();
    
    String fromHeader = extractParameter(message, "From: ", "\r\n");
    String fromTag = extractParameter(fromHeader, ";tag=", "\r");
    if (fromTag.indexOf(">") > 0) {
        fromTag = fromTag.substring(0, fromTag.indexOf(">"));
    }
    if (fromTag.indexOf(",") > 0) {
        fromTag = fromTag.substring(0, fromTag.indexOf(","));
    }
    fromTag.trim();
    
    String cseqLine = extractParameter(message, "CSeq: ", "\r\n");
    String cseqNum = cseqLine.substring(0, cseqLine.indexOf(" "));
    int cseq = cseqNum.toInt();
    
    if (realm.isEmpty() && !sipRealm.isEmpty()) {
        realm = sipRealm;
    }
    
    Serial.println("=== Authentication Challenge ===");
    Serial.println("Nonce: " + nonce);
    Serial.println("Realm: " + realm);
    Serial.println("Algorithm: " + algorithm);
    Serial.println("Call-ID: " + callID);
    Serial.println("From Tag: " + fromTag);
    Serial.println("CSeq: " + String(cseq));
    Serial.println("==============================");
    
    // Get current IP
    String localIP = ETH.localIP().toString();
    
    // Calculate digest response (RFC 2617)
    String uri = "sip:" + sipServer;
    String ha1 = calculateMD5(sipUsername + ":" + realm + ":" + sipPassword);
    String ha2 = calculateMD5("REGISTER:" + uri);
    String response = calculateMD5(ha1 + ":" + nonce + ":" + ha2);
    
    Serial.println("HA1: " + ha1);
    Serial.println("HA2: " + ha2);
    Serial.println("Response: " + response);
    
    // Generate new branch for this transaction
    String branch = "z9hG4bK" + generateTag();
    
    // Increment CSeq
    cseq++;
    
    // Build authenticated REGISTER with same Call-ID and From tag
    String registerMsg = "REGISTER sip:" + sipServer + " SIP/2.0\r\n";
    registerMsg += "Via: SIP/2.0/UDP " + localIP + ":" + String(localSipPort) + ";branch=" + branch + ";rport\r\n";
    registerMsg += "Max-Forwards: 70\r\n";
    registerMsg += "From: <sip:" + sipUsername + "@" + sipServer + ">;tag=" + fromTag + "\r\n";
    registerMsg += "To: <sip:" + sipUsername + "@" + sipServer + ">\r\n";
    registerMsg += "Call-ID: " + callID + "\r\n";
    registerMsg += "CSeq: " + String(cseq) + " REGISTER\r\n";
    registerMsg += "Contact: <sip:" + sipUsername + "@" + localIP + ":" + String(localSipPort) + ">\r\n";
    
    // Build Authorization header with all parameters properly quoted
    registerMsg += "Authorization: Digest ";
    registerMsg += "username=\"" + sipUsername + "\", ";
    registerMsg += "realm=\"" + realm + "\", ";
    registerMsg += "nonce=\"" + nonce + "\", ";
    registerMsg += "uri=\"" + uri + "\", ";
    registerMsg += "response=\"" + response + "\", ";
    registerMsg += "algorithm=" + algorithm + "\r\n";

    registerMsg += "Allow: INVITE, ACK, CANCEL, BYE, OPTIONS\r\n";
    registerMsg += "Expires: ";
    registerMsg += String(SIP_REGISTER_EXPIRES);
    registerMsg += "\r\n";
    registerMsg += "User-Agent: ";
    registerMsg += SIP_USER_AGENT;
    registerMsg += "\r\n";
    registerMsg += "Content-Length: 0\r\n";
    registerMsg += "\r\n";
    
    Serial.println("=== Sending Authenticated REGISTER ===");
    Serial.println(registerMsg);
    Serial.println("=====================================");
    
    // Send authenticated REGISTER
    this->send_sip_message(remoteIP, remotePort, registerMsg);
}

void SIPClient::handle_invite_message(String message, String remoteIP, int remotePort) {
    // Extract call details
    currentCallID = extractParameter(message, "Call-ID: ", "\r\n");
    currentFromTag = extractParameter(message, "From: ", ";tag=");
    currentFromTag = extractParameter(currentFromTag + message, ";tag=", "\r\n");
    String cseq = extractParameter(message, "CSeq: ", "\r\n");
    String via = extractParameter(message, "Via: ", "\r\n");
    String from = extractParameter(message, "From: ", "\r\n");
    String to = extractParameter(message, "To: ", "\r\n");
    String contact = extractParameter(message, "Contact: ", "\r\n");
    
    // Generate To tag
    currentToTag = generateTag();
    
    // Get current IP
    String localIP = ETH.localIP().toString();
    
    // Send 180 Ringing
    String ringing = "SIP/2.0 180 Ringing\r\n";
    ringing += "Via: " + via + "\r\n";
    ringing += "From: " + from + "\r\n";
    ringing += "To: " + to + ";tag=" + currentToTag + "\r\n";
    ringing += "Call-ID: " + currentCallID + "\r\n";
    ringing += "CSeq: " + cseq + "\r\n";
    ringing += "Contact: <sip:" + sipUsername + "@" + localIP + ":" + String(localSipPort) + ">\r\n";
    ringing += "Content-Length: 0\r\n";
    ringing += "\r\n";
    
    this->send_sip_message(remoteIP, remotePort, ringing);
}

void SIPClient::handle_bye_message(String message, String remoteIP, int remotePort) {
    String cseq = extractParameter(message, "CSeq: ", "\r\n");
    String via = extractParameter(message, "Via: ", "\r\n");
    String from = extractParameter(message, "From: ", "\r\n");
    String to = extractParameter(message, "To: ", "\r\n");
    String callID = extractParameter(message, "Call-ID: ", "\r\n");

    // Clear call state
    currentCallID = "";
    currentFromTag = "";
    currentToTag = "";
    
    // Send 200 OK
    String ok = "SIP/2.0 200 OK\r\n";
    ok += "Via: " + via + "\r\n";
    ok += "From: " + from + "\r\n";
    ok += "To: " + to + "\r\n";
    ok += "Call-ID: " + callID + "\r\n";
    ok += "CSeq: " + cseq + "\r\n";
    ok += "Content-Length: 0\r\n";
    ok += "\r\n";
    
    this->send_sip_message(remoteIP, remotePort, ok);
}

void SIPClient::handle_options_message(String message, String remoteIP, int remotePort) {
    // Extract headers from OPTIONS request
    String cseq = extractParameter(message, "CSeq: ", "\r\n");
    String via = extractParameter(message, "Via: ", "\r\n");
    String from = extractParameter(message, "From: ", "\r\n");
    String to = extractParameter(message, "To: ", "\r\n");
    String callID = extractParameter(message, "Call-ID: ", "\r\n");
    
    // Get current IP
    String localIP = ETH.localIP().toString();
    
    // Build 200 OK response with supported methods
    String ok = "SIP/2.0 200 OK\r\n";
    ok += "Via: " + via + "\r\n";
    ok += "From: " + from + "\r\n";
    ok += "To: " + to + "\r\n";
    ok += "Call-ID: " + callID + "\r\n";
    ok += "CSeq: " + cseq + "\r\n";
    ok += "Contact: <sip:" + sipUsername + "@" + localIP + ":" + String(localSipPort) + ">\r\n";
    ok += "Accept: application/sdp\r\n";
    ok += "Accept-Language: en\r\n";
    ok += "Allow: INVITE, CANCEL, BYE, OPTIONS\r\n";
    ok += "Supported: replaces, timer\r\n";
    ok += "User-Agent: ";
    ok += SIP_USER_AGENT;
    ok += "\r\n";
    ok += "Content-Length: 0\r\n";
    ok += "\r\n";
    
    this->send_sip_message(remoteIP, remotePort, ok);
    
    Serial.println("OPTIONS 200 OK sent");
}

void SIPClient::handle_sip_packet() {
    int packetSize = udpSIP.parsePacket();
    if (packetSize > 0) {
        char incomingPacket[2048];
        int len = udpSIP.read(incomingPacket, sizeof(incomingPacket) - 1);
        if (len > 0) {
            incomingPacket[len] = 0;
        }
        
        String sipMessage = String(incomingPacket);
        String remoteIP = udpSIP.remoteIP().toString();
        int remotePort = udpSIP.remotePort();
        
        Serial.println("\n=== Received SIP Message ===");
        Serial.println(sipMessage);
        Serial.println("============================\n");
        
        // Parse SIP message
        if (sipMessage.startsWith("SIP/2.0 401")) {
            // Authentication required
            Serial.println("Authentication challenge received");
            
            // Protect against infinite auth loops
            if (millis() - lastAuthAttempt < 5000 && authAttempts >= 3) {
                Serial.println("ERROR: Too many authentication failures - stopping");
                authAttempts = 0;
                return;
            }
            
            authAttempts++;
            lastAuthAttempt = millis();
            this->handle_auth_challenge(sipMessage, remoteIP, remotePort);
        } else if (sipMessage.startsWith("SIP/2.0 200")) {
            // Success response
            if (sipMessage.indexOf("REGISTER") > 0 || sipMessage.indexOf("CSeq:") > 0) {
                sipRegistered = true;
                authAttempts = 0;  // Reset auth attempts on success
                Serial.println("SIP registration successful!");
            }
        } else if (sipMessage.startsWith("INVITE")) {
            // Incoming call!
            Serial.println("*** INCOMING CALL ***");
            
            this->handle_invite_message(sipMessage, remoteIP, remotePort);
        } else if (sipMessage.startsWith("BYE") || sipMessage.startsWith("CANCEL")) {
            // Call ended
            Serial.println("*** CALL ENDED ***");
            
            this->handle_bye_message(sipMessage, remoteIP, remotePort);
        } else if (sipMessage.startsWith("OPTIONS")) {
            // OPTIONS request - respond with capabilities
            Serial.println("OPTIONS request received");
            this->handle_options_message(sipMessage, remoteIP, remotePort);
        }
    }
}

void SIPClient::handle_sip_registration() {
    if (sipRegistered && millis() - lastRegisterTime > SIP_REGISTER_INTERVAL) {
        this->begin_registration();
    }
}

void SIPClient::handle() {
    this->handle_sip_packet();
    this->handle_sip_registration();
}

SIPClient::SIPClient(int localSipPort) {
    this->sipServer = "";
    this->sipPort = 5060;
    this->localSipPort = localSipPort;
    this->sipUsername = "";
    this->sipPassword = "";
    this->sipRealm = "";

    this->sipRegistered = false;
    this->lastRegisterTime = 0;
    this->currentCallID = "";
    this->currentFromTag = "";
    this->currentToTag = "";
    this->authAttempts = 0;
    this->lastAuthAttempt = 0;

    udpSIP.begin(sipPort);
}

SIPClient::SIPClient(int localSipPort, String sipServer, int sipPort, String sipUsername, String sipPassword, String sipRealm) {
    this->sipServer = sipServer;
    this->sipPort = sipPort;
    this->localSipPort = localSipPort;
    this->sipUsername = sipUsername;
    this->sipPassword = sipPassword;
    this->sipRealm = sipRealm;

    this->sipRegistered = false;
    this->lastRegisterTime = 0;
    this->currentCallID = "";
    this->currentFromTag = "";
    this->currentToTag = "";
    this->authAttempts = 0;
    this->lastAuthAttempt = 0;

    udpSIP.begin(sipPort);
}