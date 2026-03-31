#pragma once
#include <Arduino.h>
#include "TriviaQ.h"

class OllamaClient {
public:
    OllamaClient(const char* url, const char* model, const char* prompt);

    // Override the subject and difficulty used in the prompt.
    // Pass empty strings to fall back to the built-in topic rotation.
    void setUserConfig(const String& subject, const String& difficulty);

    TriviaQ fetch();
private:
    const char* _url;
    const char* _model;
    const char* _prompt;
    uint8_t     _topicIdx  = 0; // cycles through _TOPICS when no subject override
    uint8_t     _aspectIdx = 0; // cycles through _ASPECTS when subject is fixed
    String      _subject;       // set by setUserConfig(); empty = use topic rotation
    String      _difficulty;    // set by setUserConfig(); empty = no difficulty hint

    static const char* const _ASPECTS[];
    static const uint8_t     _NUM_ASPECTS;

    static const char* const _TOPICS[];
    static const uint8_t     _NUM_TOPICS;
    static String _extractJson(const String& s);
};
