#ifndef CONFIGSTORE_H
#define CONFIGSTORE_H
#include <Arduino.h>
#include <Preferences.h>

class ConfigStore {
    private:
        Preferences preferences;
    public:
        String get_string(String key);
        String get_string(String key, String def);
        int get_integer(String key);
        int get_integer(String key, int def);
        bool get_boolean(String key);
        bool get_boolean(String key, bool def);
        void put_string(String key, String value);
        void put_integer(String key, int value);
        void put_boolean(String key, bool def);
        String get_default_string(String key);
        int get_default_integer(String key);
        bool get_default_boolean(String key);

        void init();
        void end();
};
#endif