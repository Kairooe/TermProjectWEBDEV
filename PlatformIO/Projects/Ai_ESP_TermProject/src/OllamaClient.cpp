#include "OllamaClient.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static const char* const LABELS[4] = {"A", "B", "C", "D"};

// ── Topic list ────────────────────────────────────────────────────────────────
// Each fetch() call advances to the next topic, forcing the model to vary its
// subject matter rather than defaulting to the same category repeatedly.
const char* const OllamaClient::_TOPICS[] = {
    "science and nature",
    "world history",
    "geography",
    "sports and athletics",
    "music",
    "movies and television",
    "food and cuisine",
    "animals and wildlife",
    "technology and computers",
    "literature and books",
    "space and astronomy",
    "art and culture",
};
const uint8_t OllamaClient::_NUM_TOPICS =
    sizeof(OllamaClient::_TOPICS) / sizeof(OllamaClient::_TOPICS[0]);

// ── Constructor ───────────────────────────────────────────────────────────────
OllamaClient::OllamaClient(const char* url, const char* model, const char* prompt)
    : _url(url), _model(model), _prompt(prompt) {}

// ── Private ───────────────────────────────────────────────────────────────────
String OllamaClient::_extractJson(const String& s) {
    int start = s.indexOf('{');
    int end   = s.lastIndexOf('}');
    if (start < 0 || end <= start) return "";
    return s.substring(start, end + 1);
}

// ── Public ────────────────────────────────────────────────────────────────────
TriviaQ OllamaClient::fetch() {
    TriviaQ q;

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FETCH] ERROR: WiFi not connected");
        return q;
    }

    // Pick the next topic and advance the index
    const char* topic = _TOPICS[_topicIdx % _NUM_TOPICS];
    _topicIdx++;

    // Append the topic to the base prompt
    String fullPrompt = String(_prompt) + " Topic: " + topic + ".";

    Serial.printf("[FETCH] Topic: %s\n", topic);

    HTTPClient http;
    http.begin(_url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);  // 10-second hard timeout; main.cpp retries up to 3×

    // Build request body:
    //   temperature > 1.0  → more creative / less repetitive outputs
    //   seed from hardware RNG → different result every call, even same prompt
    DynamicJsonDocument reqDoc(768);
    reqDoc["model"]                  = _model;
    reqDoc["prompt"]                 = fullPrompt;
    reqDoc["stream"]                 = false;
    reqDoc["options"]["temperature"] = 1.1;
    reqDoc["options"]["seed"]        = (int32_t)esp_random();
    String body;
    serializeJson(reqDoc, body);

    Serial.println("[FETCH] POST → Ollama ...");
    int code = http.POST(body);

    if (code != 200) {
        Serial.printf("[FETCH] HTTP error: %d\n", code);
        http.end();
        return q;
    }

    // Stream response through a filter — discards the large "context" token
    // array Ollama returns without loading it into RAM
    StaticJsonDocument<32> filter;
    filter["response"] = true;

    DynamicJsonDocument outerDoc(2048);
    WiFiClient* stream = http.getStreamPtr();
    DeserializationError err = deserializeJson(outerDoc, *stream,
                                               DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        Serial.printf("[FETCH] Outer JSON error: %s\n", err.c_str());
        return q;
    }

    String innerStr = outerDoc["response"].as<String>();
    Serial.printf("[FETCH] Model output: %s\n", innerStr.c_str());

    String jsonStr = _extractJson(innerStr);
    if (jsonStr.isEmpty()) {
        Serial.println("[FETCH] No JSON found in model output");
        return q;
    }

    DynamicJsonDocument innerDoc(1024);
    err = deserializeJson(innerDoc, jsonStr);
    if (err) {
        Serial.printf("[FETCH] Inner JSON error: %s\n", err.c_str());
        return q;
    }

    // ── JSON field validation ─────────────────────────────────────────────────
    // Reject the response if any required field is missing or empty
    const char* required[] = {"question", "A", "B", "C", "D", "answer"};
    for (const char* key : required) {
        if (!innerDoc.containsKey(key) || innerDoc[key].as<String>().isEmpty()) {
            Serial.printf("[FETCH] Bad response: missing/empty field \"%s\"\n", key);
            return q;  // q.valid stays false
        }
    }

    q.question   = innerDoc["question"].as<String>();
    q.choices[0] = innerDoc["A"].as<String>();
    q.choices[1] = innerDoc["B"].as<String>();
    q.choices[2] = innerDoc["C"].as<String>();
    q.choices[3] = innerDoc["D"].as<String>();

    String ans = innerDoc["answer"].as<String>();
    ans.trim();
    ans.toUpperCase();

    if      (ans == "A") q.correct = 0;
    else if (ans == "B") q.correct = 1;
    else if (ans == "C") q.correct = 2;
    else if (ans == "D") q.correct = 3;
    else {
        Serial.printf("[FETCH] Unexpected answer field: \"%s\"\n", ans.c_str());
        return q;
    }

    q.valid = true;

    Serial.printf("[FETCH] Question : %s\n",     q.question.c_str());
    Serial.printf("[FETCH] A) %-18s  B) %s\n",   q.choices[0].c_str(), q.choices[1].c_str());
    Serial.printf("[FETCH] C) %-18s  D) %s\n",   q.choices[2].c_str(), q.choices[3].c_str());
    Serial.printf("[FETCH] Correct  : %s) %s\n", ans.c_str(), q.choices[q.correct].c_str());

    return q;
}
