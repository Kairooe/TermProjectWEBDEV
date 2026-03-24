#include "LEDController.h"

static const char* const LABELS[4] = {"A", "B", "C", "D"};

LEDController::LEDController(const uint8_t pins[4]) {
    for (uint8_t i = 0; i < 4; i++) _pins[i] = pins[i];
}

void LEDController::begin() {
    for (uint8_t i = 0; i < 4; i++) {
        pinMode(_pins[i], OUTPUT);
        digitalWrite(_pins[i], LOW);
    }
}

void LEDController::off() {
    for (uint8_t i = 0; i < 4; i++) digitalWrite(_pins[i], LOW);
}

void LEDController::flashCorrect(uint8_t idx) {
    Serial.printf("[LED]  Rapid-flashing LED %s x6 (correct)\n", LABELS[idx]);
    for (uint8_t i = 0; i < 6; i++) {
        digitalWrite(_pins[idx], HIGH); delay(80);
        digitalWrite(_pins[idx], LOW);  delay(80);
    }
}

void LEDController::flashWrong(uint8_t correctIdx) {
    Serial.printf("[LED]  Slow-flashing all x3, then holding LED %s (correct)\n", LABELS[correctIdx]);
    for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t j = 0; j < 4; j++) digitalWrite(_pins[j], HIGH);
        delay(400);
        off();
        delay(400);
    }
    digitalWrite(_pins[correctIdx], HIGH);
    delay(2000);
    off();
}
