#pragma once
#include <Arduino.h>

// Shared data type used by OllamaClient (to produce) and OLEDDisplay (to render)
struct TriviaQ {
    String  question;
    String  choices[4]; // raw text — no A/B/C/D prefix
    uint8_t correct = 0; // 0-3
    bool    valid   = false;
};
