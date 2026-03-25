#pragma once
#include <Arduino.h>

class LEDController {
public:
    LEDController(const uint8_t pins[4]);
    void begin();
    void off();
    void flashCorrect(uint8_t idx);      // rapid-flash the correct LED
    void flashWrong(uint8_t correctIdx); // all-blink x3, then hold correct LED briefly
    void flashGameOver();                // alternating A+C / B+D blink x3
private:
    uint8_t _pins[4];
};
