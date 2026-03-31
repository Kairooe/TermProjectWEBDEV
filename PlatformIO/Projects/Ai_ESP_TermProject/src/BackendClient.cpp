#include "BackendClient.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

static const char* const LETTER[4] = {"A", "B", "C", "D"};

BackendClient::BackendClient(const char* baseUrl) : _baseUrl(baseUrl) {}

bool BackendClient::post(const char* deviceCode, const TriviaQ& q,
                         uint8_t pressedIdx, bool correct, uint8_t qNum,
                         const String& subject) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[BACKEND] WiFi not connected — skipping POST");
        return false;
    }

    // ISO8601 timestamp (real time only if NTP synced, else epoch placeholder)
    char ts[25] = "1970-01-01T00:00:00Z";
    time_t now = time(nullptr);
    if (now > 1000000000UL) {
        // FIX (thread safety): gmtime() returns a pointer to a single static struct
        // shared by all threads; on ESP32/FreeRTOS the AsyncWebServer runs on a
        // separate task and could call libc concurrently.  gmtime_r() writes into a
        // caller-supplied struct and is re-entrant safe.
        struct tm tinfo;
        gmtime_r(&now, &tinfo);
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tinfo);
    }

    DynamicJsonDocument doc(512);
    doc["code"]           = deviceCode;
    doc["question"]       = q.question;
    doc["selectedAnswer"] = LETTER[pressedIdx];
    doc["correctAnswer"]  = LETTER[q.correct];
    doc["isCorrect"]      = correct;
    doc["questionNumber"] = qNum;
    doc["subject"]        = subject.length() > 0 ? subject : "general";
    doc["timestamp"]      = ts;
    String body;
    serializeJson(doc, body);

    String url = String(_baseUrl) + "/api/device/record";

    WiFiClientSecure secureClient;
    HTTPClient http;
    if (url.startsWith("https://")) {
        secureClient.setInsecure();  // TLS-encrypted; skip CA verification (see DeviceClient.cpp)
        http.begin(secureClient, url);
    } else {
        http.begin(url);
    }
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(2000);  // 2s hard cap — don't stall the game

    Serial.printf("[BACKEND] POST → %s\n", url.c_str());
    int code = http.POST(body);
    http.end();

    if (code == 201) {
        Serial.println("[BACKEND] OK (201)");
        return true;
    }
    Serial.printf("[BACKEND] Failed: HTTP %d\n", code);
    return false;
}
