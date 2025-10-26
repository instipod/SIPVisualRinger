#ifndef SIP_H
#define SIP_H
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ETH.h>
#include <MD5Builder.h>

#define SIP_REGISTER_INTERVAL 600000 // 10 minutes
#define SIP_REGISTER_EXPIRES 900 // 15 minutes
#define SIP_USER_AGENT "ESP32-SIP/1.1"

class SIPClient {
    private:
        bool sipRegistered;
        unsigned long lastRegisterTime;
        String currentCallID;
        String currentFromTag;
        String currentToTag;
        int authAttempts;
        unsigned long lastAuthAttempt;
        
        WiFiUDP udpSIP;

    public:
        String sipServer;
        int sipPort;
        int localSipPort;
        String sipUsername;
        String sipPassword;
        String sipRealm;
        
        SIPClient(int localSipPort);
        SIPClient(int localSipPort, String sipServer, int sipPort, String sipUsername, String sipPassword, String sipRealm);

        static String generateCallID();
        static String generateTag();
        static String calculateMD5(String input);
        static String extractParameter(String message, String startDelim, String endDelim);

        bool is_registered();
        bool is_ringing();

        void begin_registration();
        void end_registration(bool networkLost);
        void update_credentials(String sipServer, int sipPort, String sipUsername, String sipPassword, String sipRealm);
        
        void send_sip_message(String remoteIP, int remotePort, String message);

        void handle_auth_challenge(String message, String remoteIP, int remotePort);
        void handle_invite_message(String message, String remoteIP, int remotePort);
        void handle_bye_message(String message, String remoteIP, int remotePort);
        void handle_options_message(String message, String remoteIP, int remotePort);
        void handle_sip_packet();
        void handle_sip_registration();
        void handle();
};
#endif