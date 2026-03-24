#pragma once
#include <Arduino.h>
#include "TriviaQ.h"

class OllamaClient {
public:
    OllamaClient(const char* url, const char* model, const char* prompt);
    TriviaQ fetch();
private:
    const char* _url;
    const char* _model;
    const char* _prompt;
    uint8_t     _topicIdx = 0; // cycles through _TOPICS each call

    static const char* const _TOPICS[];
    static const uint8_t     _NUM_TOPICS;
    static String _extractJson(const String& s);
};
