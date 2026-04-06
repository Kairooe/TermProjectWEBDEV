#include "OllamaClient.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

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

// ── Aspect list ───────────────────────────────────────────────────────────────
// When the user has set a fixed subject, we rotate through these aspects so the
// model is forced to pick a different angle each question instead of defaulting
// to the same famous fact every time.
const char* const OllamaClient::_ASPECTS[] = {
    "key events",
    "important people and leaders",
    "causes and origins",
    "consequences and aftermath",
    "technology and weapons",
    "dates and timeline",
    "battles and locations",
    "politics and decisions",
    "lesser-known facts",
    "statistics and numbers",
};
const uint8_t OllamaClient::_NUM_ASPECTS =
    sizeof(OllamaClient::_ASPECTS) / sizeof(OllamaClient::_ASPECTS[0]);

// ── Constructor ───────────────────────────────────────────────────────────────
OllamaClient::OllamaClient(const char* url, const char* model, const char* prompt)
    : _url(url), _model(model), _prompt(prompt) {}

// ── setUserConfig ─────────────────────────────────────────────────────────────
void OllamaClient::setUserConfig(const String& subject, const String& difficulty) {
    _subject    = subject;
    _difficulty = difficulty;
    Serial.printf("[FETCH] UserConfig set — subject: \"%s\"  difficulty: \"%s\"\n",
                  _subject.c_str(), _difficulty.c_str());
}

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

    // Build topic string — rotate aspects when subject is fixed so the model is
    // forced to pick a different angle every question (prevents repetition)
    String topic;
    if (_subject.length() > 0) {
        const char* aspect = _ASPECTS[_aspectIdx % _NUM_ASPECTS];
        _aspectIdx++;
        topic = _subject + ", specifically about " + aspect;
    } else {
        topic = _TOPICS[_topicIdx % _NUM_TOPICS];
        _topicIdx++;
    }

    // Build prompt: base + topic + optional difficulty instruction
    String fullPrompt = String(_prompt) + " Topic: " + topic + ".";
    if (_difficulty.length() > 0) {
        if (_difficulty == "easy")
            fullPrompt += " Use simple, well-known facts suitable for beginners.";
        else if (_difficulty == "medium")
            fullPrompt += " Use moderately challenging facts.";
        else if (_difficulty == "hard")
            fullPrompt += " Use specific, challenging facts that require deep knowledge.";
    }

    Serial.printf("[FETCH] Topic: %s  Difficulty: %s\n",
                  topic.c_str(), _difficulty.length() ? _difficulty.c_str() : "default");

    WiFiClientSecure secureClient;
    HTTPClient http;
    String urlStr(_url);
    if (urlStr.startsWith("https://")) {
        secureClient.setInsecure();
        http.begin(secureClient, urlStr);
    } else {
        http.begin(urlStr);
    }
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(65000);  // HTTPClient::setTimeout takes uint16_t (max 65535 ms) — use 65 s

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

    Serial.printf("[FETCH] POST → %s\n", urlStr.c_str());
    int code = http.POST(body);
    Serial.printf("[FETCH] HTTP code: %d\n", code);

    if (code != 200) {
        String errBody = http.getString();
        Serial.printf("[FETCH] Error body: %.200s\n", errBody.c_str());
        http.end();
        return q;
    }

    // Stream response through a filter — discards the large "context" token
    // array Ollama returns without loading it into RAM
    StaticJsonDocument<32> filter;
    filter["response"] = true;

    DynamicJsonDocument outerDoc(2048);
    WiFiClient* stream = http.getStreamPtr();
    // FIX (null pointer): getStreamPtr() returns nullptr when the connection dropped
    // between POST and read; dereferencing it without this guard causes a hard crash.
    if (!stream) {
        Serial.println("[FETCH] ERROR: stream is null — connection dropped after POST");
        http.end();
        return q;
    }
    DeserializationError err = deserializeJson(outerDoc, *stream,
                                               DeserializationOption::Filter(filter));
    http.end();
    // FIX (dangling pointer): stream points into HTTPClient's internal WiFiClient
    // which http.end() frees; clear it so any future accidental use fails loudly.
    stream = nullptr;

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
