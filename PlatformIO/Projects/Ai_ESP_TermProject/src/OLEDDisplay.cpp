#include "OLEDDisplay.h"
#include <Wire.h>

// ── Hardware ──────────────────────────────────────────────────────────────────
#define SCREEN_W     128
#define SCREEN_H      64
#define OLED_ADDR   0x3C

// Zone y-positions
// Q1 y=0, Q2 y=8  → both in hardware YELLOW zone (rows 0-15)
// Answers y=19/28/37/46 → all in hardware WHITE zone (rows 16-63)
static const int16_t ZONE_Y[NUM_ZONES] = { 0, 8, 19, 28, 37, 46 };
static const char    ANS_LBL[4]        = { 'A', 'B', 'C', 'D' };

// ── Constructor ───────────────────────────────────────────────────────────────
OLEDDisplay::OLEDDisplay()
    : _disp(SCREEN_W, SCREEN_H, &Wire, -1),
      _canvas(SCREEN_W, 8),
      _highlightZone(-1), _comboHint(false),
      _feedbackMode(false),
      _qFull(""), _qPxOff(0), _qPxMax(0), _qTimer(0), _qPhase(0),
      _lastRefreshMs(0), _zoneMode(false) {
    for (uint8_t i = 0; i < NUM_ZONES; i++) {
        _zones[i].offset = 0; _zones[i].maxOff = 0;
        _zones[i].timer  = 0; _zones[i].phase  = 0;
        _zones[i].y = ZONE_Y[i];
    }
}

bool OLEDDisplay::begin(uint8_t sdaPin, uint8_t sclPin) {
    Wire.begin(sdaPin, sclPin);
    Wire.setClock(400000);   // 400kHz I2C for ~40fps full-screen refresh
    bool ok = _disp.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    if (ok) Serial.println("[OLED] SSD1306 init OK — 128x64 I2C 0x3C @ 400kHz");
    else    Serial.println("[OLED] ERROR: SSD1306 init failed");
    return ok;
}

// ── Private helpers ───────────────────────────────────────────────────────────

// Word-wrap text into two lines, each max maxChars characters.
void OLEDDisplay::_wordWrap2(const String& text, String& l1, String& l2, int maxChars) {
    if ((int)text.length() <= maxChars) { l1 = text; l2 = ""; return; }
    int end = maxChars;
    while (end > 0 && text.charAt(end) != ' ') end--;
    if (end == 0) end = maxChars;   // no space: hard break
    l1 = text.substring(0, end);
    int start2 = (text.charAt(end) == ' ') ? end + 1 : end;
    l2 = text.substring(start2);
}

// Set zone content and reset its scroll state.
void OLEDDisplay::_setZone(uint8_t idx, const String& text) {
    ScrollZone& z = _zones[idx];
    z.text    = text;
    z.offset  = 0;
    int16_t raw = (int16_t)(text.length() * CHAR_W) - (int16_t)ZONE_W;
    z.maxOff  = (raw > 0) ? raw : 0;
    z.phase   = 0;
    z.timer   = millis();
}

// Advance scroll state machine for one zone.
void OLEDDisplay::_updateScroll(ScrollZone& z, uint32_t now) {
    if (z.maxOff <= 0) return;
    uint32_t elapsed = now - z.timer;
    switch (z.phase) {
        case 0:  // pause at start
            if (elapsed >= SCROLL_PAUSE) { z.phase = 1; z.timer = now; }
            break;
        case 1:  // scrolling: 2px per 16ms ≈ 125px/s
            z.offset = (int16_t)min((uint32_t)z.maxOff, elapsed * 2 / 16);
            if (z.offset >= z.maxOff) { z.offset = z.maxOff; z.phase = 2; z.timer = now; }
            break;
        case 2:  // pause at end
            if (elapsed >= SCROLL_PAUSE) { z.offset = 0; z.phase = 0; z.timer = now; }
            break;
    }
}

// Render one zone using the shared GFXcanvas1 scratch buffer.
// inverted=true → white background + black text (answer highlight).
void OLEDDisplay::_drawZone(const ScrollZone& z, bool inverted) {
    if (z.text.isEmpty()) return;

    _canvas.fillScreen(inverted ? 1 : 0);
    _canvas.setTextColor(inverted ? 0 : 1);
    _canvas.setTextSize(1);
    _canvas.setCursor(-z.offset, 0);
    _canvas.print(z.text);

    _disp.drawBitmap(0, z.y, _canvas.getBuffer(), SCREEN_W, 8,
                     inverted ? SSD1306_BLACK : SSD1306_WHITE,
                     inverted ? SSD1306_WHITE : SSD1306_BLACK);
}

// ── Shared Q-line scroll state machine ───────────────────────────────────────
// Mirrors _updateScroll() but drives _qPxOff/_qPxMax/_qPhase/_qTimer.
void OLEDDisplay::_updateQScroll(uint32_t now) {
    if (_qPxMax <= 0) return;
    uint32_t elapsed = now - _qTimer;
    switch (_qPhase) {
        case 0:
            if (elapsed >= SCROLL_PAUSE) { _qPhase = 1; _qTimer = now; }
            break;
        case 1:
            _qPxOff = (int16_t)min((uint32_t)_qPxMax, elapsed * 2 / 16);
            if (_qPxOff >= _qPxMax) { _qPxOff = _qPxMax; _qPhase = 2; _qTimer = now; }
            break;
        case 2:
            if (elapsed >= SCROLL_PAUSE) { _qPxOff = 0; _qPhase = 0; _qTimer = now; }
            break;
    }
}

// Draw the two Q lines (zones 0 and 1) onto the display.
//
// Three modes:
//   feedback  → zones[0/1].text drawn statically via _drawZone (set by setFeedback)
//   static    → _qFull fits in 42 chars; word-wrap to two static lines
//   scrolling → sliding 42-char window: L1 starts at charOff, L2 at charOff+21,
//               both offset by the same subPx for pixel-smooth sync
void OLEDDisplay::_drawQLines() {
    if (_feedbackMode) {
        _drawZone(_zones[0], false);
        _drawZone(_zones[1], false);
        return;
    }

    String  l1text, l2text;
    int16_t subPx = 0;

    if (_qPxMax <= 0) {
        // Static: question fits in two lines of 21 chars each
        _wordWrap2(_qFull, l1text, l2text, 21);
    } else {
        // Scrolling: compute sliding-window character and sub-pixel offsets
        int16_t charOff = _qPxOff / (int16_t)CHAR_W;
        subPx           = _qPxOff % (int16_t)CHAR_W;
        l1text = _qFull.substring((uint16_t)charOff);
        l2text = _qFull.substring((uint16_t)(charOff + 21));
    }

    // Combo hint overrides line 2
    if (_comboHint) l2text = "Hold A+B...";

    // Line 1 — both lines share the same subPx scroll offset
    _canvas.fillScreen(0);
    _canvas.setTextColor(1);
    _canvas.setTextSize(1);
    _canvas.setCursor(-subPx, 0);
    _canvas.print(l1text);
    _disp.drawBitmap(0, _zones[0].y, _canvas.getBuffer(), SCREEN_W, 8,
                     SSD1306_WHITE, SSD1306_BLACK);

    // Line 2 — combo hint always starts at x=0 (no scroll offset)
    _canvas.fillScreen(0);
    _canvas.setCursor(_comboHint ? 0 : -subPx, 0);
    _canvas.print(l2text);
    _disp.drawBitmap(0, _zones[1].y, _canvas.getBuffer(), SCREEN_W, 8,
                     SSD1306_WHITE, SSD1306_BLACK);
}

// ── tick() — call every loop iteration ───────────────────────────────────────
void OLEDDisplay::tick() {
    if (!_zoneMode) return;
    uint32_t now = millis();
    if (now - _lastRefreshMs < REFRESH_MS) return;
    _lastRefreshMs = now;

    // Advance scroll states
    if (_feedbackMode) {
        _updateScroll(_zones[0], now);   // "CORRECT!"/"WRONG!" — won't scroll (short)
        _updateScroll(_zones[1], now);   // "Ans: ..." — may scroll if long
    } else {
        _updateQScroll(now);             // shared Q-line scroll
    }
    for (uint8_t i = 2; i < NUM_ZONES; i++) _updateScroll(_zones[i], now);

    _disp.clearDisplay();
    _disp.setTextColor(SSD1306_WHITE);

    // Separator between Q area (rows 0-15) and answer area (rows 16-63)
    _disp.drawFastHLine(0, 17, SCREEN_W, SSD1306_WHITE);

    _drawQLines();

    for (uint8_t i = 2; i < NUM_ZONES; i++) {
        bool inv = ((int8_t)(i - 2) == _highlightZone);
        _drawZone(_zones[i], inv);
    }

    _disp.display();
}

// ── Zone-mode screen management ───────────────────────────────────────────────

void OLEDDisplay::loadQuestion(const TriviaQ& q, uint8_t qNum) {
    // Build full question string; shared Q scroll uses this directly
    _qFull = String("Q") + String(qNum) + " " + q.question;

    // Max scroll = chars beyond the 42-char window × pixels-per-char
    int16_t excess = (int16_t)_qFull.length() - 42;
    _qPxMax = (excess > 0) ? (int16_t)(excess * CHAR_W) : 0;
    _qPxOff = 0;
    _qPhase = 0;
    _qTimer = millis();

    // Answer zones: "A choice text" (letter + space + text)
    for (uint8_t i = 0; i < 4; i++) {
        String ans = String(ANS_LBL[i]) + " " + q.choices[i];
        _setZone(2 + i, ans);
    }

    _highlightZone = -1;
    _comboHint     = false;
    _feedbackMode  = false;
    _zoneMode      = true;
    _lastRefreshMs = 0;  // force immediate redraw
}

void OLEDDisplay::setHighlight(uint8_t ansIdx) {
    _highlightZone = (int8_t)ansIdx;  // 0-3
}

void OLEDDisplay::clearHighlight() {
    _highlightZone = -1;
}

void OLEDDisplay::setFeedback(bool correct, uint8_t pressedIdx, const TriviaQ& q) {
    // Replace Q-scroll with static result text in zones[0/1]
    _setZone(0, correct ? "CORRECT!" : "WRONG!");
    if (correct) {
        _setZone(1, "");
        _highlightZone = (int8_t)pressedIdx;   // keep correct answer highlighted
    } else {
        String ans2 = String("Ans: ") + ANS_LBL[q.correct] + " " + q.choices[q.correct];
        _setZone(1, ans2);
        _highlightZone = (int8_t)q.correct;    // highlight correct answer zone
    }
    _feedbackMode  = true;
    _comboHint     = false;
    _lastRefreshMs = 0;
}

void OLEDDisplay::setComboHint(bool show) {
    if (show == _comboHint) return;
    _comboHint = show;
    // _drawQLines() checks _comboHint to override line 2 on next tick
}

void OLEDDisplay::setFeedbackNote(const char* text) {
    // Replaces zone[1] content — call after setFeedback() to show e.g. "Saved!"
    _setZone(1, String(text));
}

// ── Static screens (bypass zone system) ──────────────────────────────────────

void OLEDDisplay::showSplash(const char* statusLine) {
    _zoneMode = false;
    _disp.clearDisplay();
    _disp.setTextColor(SSD1306_WHITE);

    // "Digital" — textSize(2), centered (12px/char)
    _disp.setTextSize(2);
    int16_t x = (int16_t)((SCREEN_W - 7 * 12) / 2);
    _disp.setCursor(x > 0 ? x : 0, 2);
    _disp.print("Digital");

    // "Flashcard" — textSize(2), centered
    x = (int16_t)((SCREEN_W - 9 * 12) / 2);
    _disp.setCursor(x > 0 ? x : 0, 20);
    _disp.print("Flashcard");

    // Divider
    _disp.drawFastHLine(0, 39, SCREEN_W, SSD1306_WHITE);

    // Status line — textSize(1)
    _disp.setTextSize(1);
    _disp.setCursor(0, 44);
    _disp.print(statusLine);

    _disp.display();
}

void OLEDDisplay::showStatus(const char* title, const char* line1, const char* line2) {
    _zoneMode = false;
    _disp.clearDisplay();
    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);

    // Centred title
    int16_t tx = (int16_t)((SCREEN_W - (int16_t)(strlen(title) * CHAR_W)) / 2);
    if (tx < 0) tx = 0;
    _disp.setCursor(tx, 0);
    _disp.print(title);
    _disp.drawFastHLine(0, 10, SCREEN_W, SSD1306_WHITE);

    if (line1) { _disp.setCursor(0, 18); _disp.print(line1); }
    if (line2) { _disp.setCursor(0, 28); _disp.print(line2); }
    _disp.display();
}

void OLEDDisplay::showSummaryScore(int total, int correct) {
    _zoneMode = false;
    _disp.clearDisplay();
    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);

    _disp.setCursor(0,  0); _disp.print("=== GAME OVER ===");
    _disp.drawFastHLine(0, 10, SCREEN_W, SSD1306_WHITE);
    _disp.setCursor(0, 16); _disp.print("Score: "); _disp.print(correct);
    _disp.print(" / ");    _disp.print(total);
    int pct = (total > 0) ? correct * 100 / total : 0;
    _disp.setCursor(0, 32); _disp.print("Accuracy: "); _disp.print(pct); _disp.print("%");
    _disp.display();
}

void OLEDDisplay::showSummaryStreak(int best) {
    _zoneMode = false;
    _disp.clearDisplay();
    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);

    _disp.setCursor(0,  0); _disp.print("=== GAME OVER ===");
    _disp.drawFastHLine(0, 10, SCREEN_W, SSD1306_WHITE);
    _disp.setCursor(0, 16); _disp.print("Best streak:");
    _disp.setCursor(0, 26); _disp.print(best); _disp.print(" in a row!");
    _disp.display();
}

void OLEDDisplay::showSummaryRestart() {
    _zoneMode = false;
    _disp.clearDisplay();
    _disp.setTextSize(1);
    _disp.setTextColor(SSD1306_WHITE);

    _disp.setCursor(0,  0); _disp.print("=== GAME OVER ===");
    _disp.drawFastHLine(0, 10, SCREEN_W, SSD1306_WHITE);
    _disp.setCursor(0, 24); _disp.print("Press any button");
    _disp.setCursor(0, 34); _disp.print("   to restart");
    _disp.display();
}
