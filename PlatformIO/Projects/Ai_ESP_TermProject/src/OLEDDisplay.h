#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "TriviaQ.h"

// ===== DISPLAY =====
// Zone-based display driver.
// Q lines (zones 0-1, y=0 and y=8) fall in the hardware YELLOW zone (rows 0-15).
// Answer zones (2-5, y=19/28/37/46) fall in the hardware WHITE zone (rows 16-63).
// Q lines share ONE scroll offset and always advance together.
// Each answer zone scrolls independently.

#define NUM_ZONES 6

struct ScrollZone {
    String   text;
    int16_t  offset;
    int16_t  maxOff;
    uint32_t timer;
    uint8_t  phase;
    int16_t  y;
};

class OLEDDisplay {
public:
    OLEDDisplay();
    bool begin(uint8_t sdaPin, uint8_t sclPin);

    void loadQuestion(const TriviaQ& q, uint8_t qNum);
    void setHighlight(uint8_t ansIdx);
    void clearHighlight();
    void setFeedback(bool correct, uint8_t pressedIdx, const TriviaQ& q);
    void setComboHint(bool show);
    void setFeedbackNote(const char* text);  // override zone[1] during feedback (e.g. "Saved!")
    void tick();

    void showStatus(const char* title, const char* line1, const char* line2 = nullptr);
    void showSummaryScore(int total, int correct);
    void showSummaryStreak(int best);
    void showSummaryRestart();

private:
    Adafruit_SSD1306 _disp;
    GFXcanvas1       _canvas;

    ScrollZone  _zones[NUM_ZONES];  // [0]=Q1 [1]=Q2 [2-5]=A-D
    int8_t      _highlightZone;     // -1=none; 0-3 maps to zones 2-5
    bool        _comboHint;
    bool        _feedbackMode;      // true → draw zones[0/1].text statically (setFeedback)

    // Shared Q-line scroll state (both Q lines advance together)
    String   _qFull;     // full question string ("Q# text...")
    int16_t  _qPxOff;    // current shared pixel offset
    int16_t  _qPxMax;    // max pixel offset (0 = static)
    uint32_t _qTimer;
    uint8_t  _qPhase;    // 0=pause-start  1=scrolling  2=pause-end

    uint32_t _lastRefreshMs;
    bool     _zoneMode;

    static const uint32_t REFRESH_MS   = 25;
    static const uint32_t SCROLL_PAUSE = 1500;
    static const uint16_t ZONE_W       = 128;
    static const uint8_t  CHAR_W       = 6;

    void _setZone(uint8_t idx, const String& text);
    void _updateQScroll(uint32_t now);
    void _drawQLines();
    void _updateScroll(ScrollZone& z, uint32_t now);
    void _drawZone(const ScrollZone& z, bool inverted);
    static void _wordWrap2(const String& text, String& l1, String& l2, int maxChars);
};
