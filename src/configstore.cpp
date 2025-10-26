#include <configstore.h>

String ConfigStore::get_default_string(String key) {
    if (key == "deviceHostname") {
        return "VisualAlert-ffff";
    }
    if (key == "webPassword") {
        return "admin";
    }
    return "";
}

int ConfigStore::get_default_integer(String key) {
    if (key == "sipPort1" || key == "sipPort2") {
        return 5060;
    }
    return 0;
}

bool ConfigStore::get_default_boolean(String key) {
    if (key == "lldpEnabled" || key == "mdnsEnabled") {
        return true;
    }
    return false;
}

String ConfigStore::get_string(String key) {
    return this->get_string(key, this->get_default_string(key));
}

String ConfigStore::get_string(String key, String def) {
    return this->preferences.getString(key.c_str(), def.c_str());
}

int ConfigStore::get_integer(String key) {
    return this->get_integer(key, this->get_default_integer(key));
}

int ConfigStore::get_integer(String key, int def) {
    return this->preferences.getInt(key.c_str(), def);
}

bool ConfigStore::get_boolean(String key) {
    return this->get_boolean(key, this->get_default_boolean(key));
}

bool ConfigStore::get_boolean(String key, bool def) {
    return this->preferences.getBool(key.c_str(), def);
}

void ConfigStore::put_string(String key, String value) {
    this->preferences.putString(key.c_str(), value.c_str());
}

void ConfigStore::put_integer(String key, int value) {
    this->preferences.putInt(key.c_str(), value);
}

void ConfigStore::put_boolean(String key, bool value) {
    this->preferences.putBool(key.c_str(), value);
}

void ConfigStore::init() {
    this->preferences.begin("config", false);
}

void ConfigStore::end() {
    this->preferences.end();
}