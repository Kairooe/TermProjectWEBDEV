#pragma once
#include <Arduino.h>

class ButtonHandler {
public:
    ButtonHandler(const uint8_t pins[4]);
    void   begin();
    int8_t waitForPress(); // blocks until clean press+release; returns 0-3 (A-D)
    int8_t check();        // non-blocking; returns 0-3 on press, -1 if nothing
private:
    uint8_t _pins[4];
};
