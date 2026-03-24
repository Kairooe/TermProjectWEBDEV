#include "OLEDDisplay.h"
#include <Wire.h>

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS 0x3C

static const char* const LABELS[4] = {"A", "B", "C", "D"};

// _disp is constructed with &Wire — Wire.begin() is called in begin() before use
OLEDDisplay::OLEDDisplay()
    : _disp(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {}

bool OLEDDisplay::begin(uint8_t sdaPin, uint8_t sclPin) {
    Wire.begin(sdaPin, sclPin);
    bool ok = _disp.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    if (ok) Serial.println("[OLED] SSD1306 init OK — 128x64, I2C 0x3C");
    else    Serial.println("[OLED] ERROR: SSD1306 init failed");
    return ok;
}

// ── Private ───────────────────────────────────────────────────────────────────

// Yellow zone (y=0–15): horizontally centred, size-2 text (exactly 16 px tall)
void OLEDDisplay::_yellowZone(const char* text) {
    int16_t x = (SCREEN_WIDTH - (int16_t)(strlen(text) * 12)) / 2;
    if (x < 0) x = 0;
    _disp.setTextSize(2);
    _disp.setTextColor(SSD1306_WHITE);
    _disp.setCursor(x, 0);
    _disp.print(text);
}

// ── Public ────────────────────────────────────────────────────────────────────

void OLEDDisplay::showStatus(const char* title, const char* line1, const char* line2) {
    _disp.clearDisplay();
    _yellowZone(title);
    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);
    if (line1) { _disp.setCursor(0, 22); _disp.println(line1); }
    if (line2) { _disp.setCursor(0, 32); _disp.println(line2); }
    _disp.display();
}

void OLEDDisplay::showQuestion(const TriviaQ& q, uint8_t n, uint16_t scrollOffset) {
    char title[6];
    snprintf(title, sizeof(title), "Q%d", n);

    _disp.clearDisplay();
    _yellowZone(title);

    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);

    // Question text: fixed 2-line area (y=17, y=25), 21 chars per line.
    // scrollOffset slides the visible character window so long questions don't
    // overflow into the choices area.  Short questions are shown statically.
    uint16_t qLen  = q.question.length();
    uint16_t start = (scrollOffset < qLen) ? scrollOffset : 0;
    String   line1 = q.question.substring(start,      start + 21);
    String   line2 = q.question.substring(start + 21, start + 42);
    _disp.setCursor(0, 17); _disp.print(line1);
    _disp.setCursor(0, 25); _disp.print(line2);

    // Choices at fixed rows — guaranteed below the 2-line question block
    _disp.setCursor(0, 33); _disp.print("A) "); _disp.println(q.choices[0]);
    _disp.setCursor(0, 41); _disp.print("B) "); _disp.println(q.choices[1]);
    _disp.setCursor(0, 49); _disp.print("C) "); _disp.println(q.choices[2]);
    _disp.setCursor(0, 57); _disp.print("D) "); _disp.println(q.choices[3]);

    _disp.display();
}

void OLEDDisplay::showFeedback(bool correct, uint8_t pressedIdx, const TriviaQ& q) {
    _disp.clearDisplay();
    _yellowZone(correct ? "CORRECT!" : "WRONG!");

    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);

    if (correct) {
        _disp.setCursor(0, 22); _disp.println("Great answer!");
        _disp.setCursor(0, 34);
        _disp.print(LABELS[q.correct]); _disp.print(") ");
        _disp.println(q.choices[q.correct]);
    } else {
        _disp.setCursor(0, 22);
        _disp.print("You:  "); _disp.print(LABELS[pressedIdx]);
        _disp.print(") "); _disp.println(q.choices[pressedIdx]);
        _disp.setCursor(0, 34);
        _disp.print("Ans:  "); _disp.print(LABELS[q.correct]);
        _disp.print(") "); _disp.println(q.choices[q.correct]);
    }
    _disp.display();
}
