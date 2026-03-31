#include "DeviceClient.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

DeviceClient::DeviceClient(const char* baseUrl) : _baseUrl(baseUrl) {}

// ── _beginHttp ────────────────────────────────────────────────────────────────
// Starts the HTTPClient on http or https depending on the base URL.
// For https we use setInsecure() — Cloudflare's cert is valid, but storing a
// root CA bundle on the ESP32 would require maintenance as certs rotate.
// The transport is still fully TLS-encrypted; we just skip cert pinning.
static bool _beginHttp(HTTPClient& http, WiFiClientSecure& secureClient, const String& url) {
    if (url.startsWith("https://")) {
        secureClient.setInsecure();  // encrypted but no CA verification
        return http.begin(secureClient, url);
    }
    return http.begin(url);
}

// ── registerCode ──────────────────────────────────────────────────────────────
bool DeviceClient::registerCode(const char* code) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[DEVICE] registerCode: WiFi not connected");
        return false;
    }

    String url = String(_baseUrl) + "/api/device/register";

    StaticJsonDocument<64> doc;
    doc["code"] = code;
    String body;
    serializeJson(doc, body);

    WiFiClientSecure secureClient;
    HTTPClient http;
    if (!_beginHttp(http, secureClient, url)) {
        Serial.println("[DEVICE] registerCode: http.begin failed");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    int code_ = http.POST(body);
    http.end();

    if (code_ == 201) {
        Serial.printf("[DEVICE] Registered code: %s\n", code);
        return true;
    }
    Serial.printf("[DEVICE] Register failed: HTTP %d\n", code_);
    return false;
}

// ── poll ──────────────────────────────────────────────────────────────────────
int8_t DeviceClient::poll(const char* code, String& outUsername,
                          String& outSubject, String& outDifficulty) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[DEVICE] poll: WiFi not connected");
        return 0;
    }

    String url = String(_baseUrl) + "/api/device/poll/" + code;

    WiFiClientSecure secureClient;
    HTTPClient http;
    if (!_beginHttp(http, secureClient, url)) {
        Serial.println("[DEVICE] poll: http.begin failed");
        return 0;
    }
    http.setTimeout(5000);

    int statusCode = http.GET();

    if (statusCode == 404) {
        http.end();
        Serial.println("[DEVICE] Poll: session expired or not found");
        return -1;
    }

    if (statusCode != 200) {
        http.end();
        Serial.printf("[DEVICE] Poll error: HTTP %d\n", statusCode);
        return 0;
    }

    // userConfig can contain strings — 512 bytes is enough
    StaticJsonDocument<512> respDoc;
    DeserializationError err = deserializeJson(respDoc, http.getString());
    http.end();

    if (err) {
        Serial.printf("[DEVICE] Poll JSON error: %s\n", err.c_str());
        return 0;
    }

    if (respDoc["claimed"].as<bool>()) {
        outUsername = respDoc["username"] | "";
        if (outUsername.isEmpty()) outUsername = "Player";

        if (respDoc.containsKey("userConfig") && !respDoc["userConfig"].isNull()) {
            outSubject    = respDoc["userConfig"]["subject"]    | "";
            outDifficulty = respDoc["userConfig"]["difficulty"] | "";
        }

        Serial.printf("[DEVICE] Claimed by: %s  subject: \"%s\"  difficulty: \"%s\"\n",
                      outUsername.c_str(), outSubject.c_str(), outDifficulty.c_str());
        return 1;
    }

    return 0;
}
