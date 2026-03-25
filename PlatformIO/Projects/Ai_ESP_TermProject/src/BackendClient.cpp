#include "BackendClient.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

static const char* const LETTER[4] = {"A", "B", "C", "D"};

BackendClient::BackendClient(const char* url) : _url(url) {}

bool BackendClient::post(const String& username, const TriviaQ& q,
                         uint8_t pressedIdx, bool correct, uint8_t qNum) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[BACKEND] WiFi not connected — skipping POST");
        return false;
    }

    // ISO8601 timestamp (real time only if NTP synced, else epoch placeholder)
    char ts[25] = "1970-01-01T00:00:00Z";
    time_t now = time(nullptr);
    if (now > 1000000000UL) {
        struct tm* t = gmtime(&now);
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", t);
    }

    DynamicJsonDocument doc(512);
    doc["username"]       = username;
    doc["question"]       = q.question;
    doc["selectedAnswer"] = LETTER[pressedIdx];
    doc["correctAnswer"]  = LETTER[q.correct];
    doc["isCorrect"]      = correct;
    doc["questionNumber"] = qNum;
    doc["timestamp"]      = ts;
    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.begin(_url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(2000);  // 2s hard cap — don't stall the game

    Serial.printf("[BACKEND] POST → %s\n", _url);
    int code = http.POST(body);
    http.end();

    if (code == 200 || code == 201) {
        Serial.printf("[BACKEND] OK (%d)\n", code);
        return true;
    }
    Serial.printf("[BACKEND] Failed: HTTP %d\n", code);
    return false;
}
