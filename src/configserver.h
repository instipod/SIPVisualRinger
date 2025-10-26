#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <ETH.h>
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include <sip.h>
#include <runtime.h>

class ConfigServer {
    private:
        Runtime &runtime;
        WebServer server = WebServer(80);

        String encrypt_cookie(String data);
        String decrypt_cookie(String data);
        String create_auth_cookie(String username);
    public:
        unsigned long sessionTimeout = 3600000; // 1 hour
        String authSecret;

        ConfigServer(Runtime &r) : runtime(r) {}

        void init();
        void handle();
        bool is_authenticated();
        void redirect_to_login();
};

#endif // WEB_SERVER_H