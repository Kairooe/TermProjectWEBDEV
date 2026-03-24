#pragma once
#include <Arduino.h>

class LEDController {
public:
    LEDController(const uint8_t pins[4]);
    void begin();
    void off();
    void flashCorrect(uint8_t idx);      // rapid-flash the pressed LED (correct answer)
    void flashWrong(uint8_t correctIdx); // slow-flash all x3, then hold correct LED
private:
    uint8_t _pins[4];
};
