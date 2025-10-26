#include <runtime.h>

void Runtime::load_configuration() {
    deviceHostname = configStore.get_string("hostname", "VisualAlert-" + ethernetMAC.substring(ethernetMAC.length() - 5, ethernetMAC.length() - 1));
    
    sipLine1.update_credentials(configStore.get_string("sipServer1"), configStore.get_integer("sipPort1"),
        configStore.get_string("sipUsername1"), configStore.get_string("sipPassword1"), configStore.get_string("sipRealm1"));
    sipLine2.update_credentials(configStore.get_string("sipServer2"), configStore.get_integer("sipPort2"),
        configStore.get_string("sipUsername2"), configStore.get_string("sipPassword2"), configStore.get_string("sipRealm2"));

    lldp.enabled = configStore.get_boolean("lldpEnabled");
    
    webPassword = configStore.get_string("web_password");
}

void Runtime::save_configuration() {
    configStore.put_string("hostname", deviceHostname);

    configStore.put_string("sipServer1", sipLine1.sipServer);
    configStore.put_integer("sipPort1", sipLine1.sipPort);
    configStore.put_string("sipUsername1", sipLine1.sipUsername);
    configStore.put_string("sipPassword1", sipLine1.sipPassword);
    configStore.put_string("sipRealm1", sipLine1.sipRealm);

    configStore.put_string("sipServer2", sipLine2.sipServer);
    configStore.put_integer("sipPort2", sipLine2.sipPort);
    configStore.put_string("sipUsername2", sipLine2.sipUsername);
    configStore.put_string("sipPassword2", sipLine2.sipPassword);
    configStore.put_string("sipRealm2", sipLine2.sipRealm);

    configStore.put_boolean("lldpEnabled", lldp.enabled);
    
    configStore.put_string("web_password", webPassword);

    ETH.setHostname(deviceHostname.c_str());
}

void Runtime::init() {
    // Enable the internal voltage reference as random seed
    // Disables WiFi and BLE
    bootloader_random_enable();

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    // The entropy context will automatically use ESP32's hardware RNG
    // which includes the SAR ADC and other entropy sources via esp_fill_random()
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    NULL, 0);

    if (ret != 0) {
        Serial.printf("Failed to initialize CTR-DRBG: %d\n", ret);
    } else {
        Serial.println("Cryptographic RNG initialized with hardware entropy");
    }
}

void Runtime::handle() {
    sipLine1.handle();
    sipLine2.handle();

    lldp.handle();
}