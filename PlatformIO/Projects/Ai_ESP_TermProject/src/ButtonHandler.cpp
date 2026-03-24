#include "ButtonHandler.h"

ButtonHandler::ButtonHandler(const uint8_t pins[4]) {
    for (uint8_t i = 0; i < 4; i++) _pins[i] = pins[i];
}

void ButtonHandler::begin() {
    for (uint8_t i = 0; i < 4; i++) pinMode(_pins[i], INPUT_PULLUP);
}

int8_t ButtonHandler::waitForPress() {
    // Wait until all buttons are released (avoids carry-over from prior press)
    bool held;
    do {
        held = false;
        for (uint8_t i = 0; i < 4; i++)
            if (digitalRead(_pins[i]) == LOW) held = true;
        if (held) delay(10);
    } while (held);

    // Poll for a clean press + release with debounce
    while (true) {
        for (uint8_t i = 0; i < 4; i++) {
            if (digitalRead(_pins[i]) == LOW) {
                delay(30);
                if (digitalRead(_pins[i]) == LOW) {
                    while (digitalRead(_pins[i]) == LOW) delay(5);
                    return (int8_t)i;
                }
            }
        }
        delay(5);
    }
}

int8_t ButtonHandler::check() {
    for (uint8_t i = 0; i < 4; i++) {
        if (digitalRead(_pins[i]) == LOW) {
            delay(30);
            if (digitalRead(_pins[i]) == LOW) {
                while (digitalRead(_pins[i]) == LOW) delay(5);
                return (int8_t)i;
            }
        }
    }
    return -1;
}
