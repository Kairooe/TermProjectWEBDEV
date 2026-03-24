#include "WiFiManager.h"
#include <WiFi.h>

static const char*    SSID       = "Jimmy3379";
static const char*    PASSWORD   = "Bkl2610/0812";
static const uint32_t TIMEOUT_MS = 10000;

void WiFiManager::begin() {
    Serial.println();
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(SSID);

    WiFi.begin(SSID, PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        _connected = true;
        Serial.println("[WiFi] Connected!");
        Serial.print("[WiFi] IP:      "); Serial.println(WiFi.localIP());
        Serial.print("[WiFi] RSSI:    "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
        Serial.print("[WiFi] MAC:     "); Serial.println(WiFi.macAddress());
        Serial.print("[WiFi] Time:    "); Serial.print(millis() - start); Serial.println("ms");
    } else {
        _connected = false;
        Serial.println("[WiFi] Timed out after 10s — continuing without WiFi.");
        WiFi.disconnect(true);
    }
}

String WiFiManager::ip() const {
    return WiFi.localIP().toString();
}
