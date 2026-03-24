#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "TriviaQ.h"

class OLEDDisplay {
public:
    OLEDDisplay();
    bool begin(uint8_t sdaPin, uint8_t sclPin);
    void showStatus(const char* title, const char* line1, const char* line2 = nullptr);
    void showQuestion(const TriviaQ& q, uint8_t n, uint16_t scrollOffset = 0);
    void showFeedback(bool correct, uint8_t pressedIdx, const TriviaQ& q);
private:
    Adafruit_SSD1306 _disp;
    void _yellowZone(const char* text); // centred size-2 text in the yellow top band
};
