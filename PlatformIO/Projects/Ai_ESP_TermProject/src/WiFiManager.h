#pragma once
#include <Arduino.h>

class WiFiManager {
public:
    // Attempts to connect to WiFi. Prints status to Serial.
    // Returns after TIMEOUT_MS regardless — never halts the program.
    void   begin();
    bool   connected() const { return _connected; }
    String ip()        const;

private:
    bool _connected = false;
};
