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

// Rapid-flash the correct answer LED 6× (~1 second total)
void LEDController::flashCorrect(uint8_t idx) {
    Serial.printf("[LED] Correct — rapid-flashing LED %s\n", LABELS[idx]);
    off();
    for (uint8_t i = 0; i < 6; i++) {
        digitalWrite(_pins[idx], HIGH); delay(80);
        digitalWrite(_pins[idx], LOW);  delay(80);
    }
}

// All-LEDs blink 3× then briefly hold correct LED (~1.5 second total)
void LEDController::flashWrong(uint8_t correctIdx) {
    Serial.printf("[LED] Wrong — blinking all x3, holding LED %s\n", LABELS[correctIdx]);
    off();
    for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t j = 0; j < 4; j++) digitalWrite(_pins[j], HIGH);
        delay(200);
        off();
        delay(200);
    }
    // Briefly hold the correct answer LED so the player can see it
    digitalWrite(_pins[correctIdx], HIGH);
    delay(600);
    off();
}

// Alternating A+C / B+D blink 3× — game-over celebration/farewell pattern
void LEDController::flashGameOver() {
    off();
    for (uint8_t i = 0; i < 3; i++) {
        digitalWrite(_pins[0], HIGH); digitalWrite(_pins[2], HIGH);
        digitalWrite(_pins[1], LOW);  digitalWrite(_pins[3], LOW);
        delay(200);
        digitalWrite(_pins[0], LOW);  digitalWrite(_pins[2], LOW);
        digitalWrite(_pins[1], HIGH); digitalWrite(_pins[3], HIGH);
        delay(200);
    }
    off();
}
