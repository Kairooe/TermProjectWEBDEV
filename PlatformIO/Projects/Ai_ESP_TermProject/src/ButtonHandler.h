#pragma once
#include <Arduino.h>

// ===== INPUT =====
// Two-phase combo detection for A+B game-over trigger.
// C and D register answers immediately on press (50ms debounce).
// A and B open a 200ms window: if the other joins → COMBO_ACTIVE;
// if released solo within 400ms → answer registered.

class ButtonHandler {
public:
    enum State { WAITING, COMBO_WINDOW, COMBO_ACTIVE, ANSWER_LOCKED };

    ButtonHandler(const uint8_t pins[4]);
    void   begin();
    void   update();             // call every loop tick — drives debounce + state machine
    int8_t getAnswer();          // 0-3 when answer ready (auto-sets ANSWER_LOCKED), else -1
    bool   isComboConfirmed();   // true once A+B held >= COMBO_HOLD_MS
    bool   isComboHinting();     // true if combo in progress >= HINT_MS (show "Hold..." on OLED)
    void   unlock();             // ANSWER_LOCKED → WAITING (call after next question loaded)
    void   clearAll();           // full reset (restart)
    State  getState() const { return _state; }
    int8_t waitForAnyPress();    // blocking — startup only

private:
    static const uint32_t DEBOUNCE_MS     = 50;
    static const uint32_t COMBO_WINDOW_MS = 200;  // B must join within this after A pressed
    static const uint32_t ANSWER_MAX_MS   = 400;  // max hold for A/B solo tap to count as answer
    static const uint32_t COMBO_HOLD_MS   = 1500; // both held this long → game-over
    static const uint32_t HINT_MS         = 500;  // show "Hold..." after this ms in combo

    uint8_t  _pins[4];
    bool     _raw[4];
    bool     _stable[4];
    uint32_t _debounceAt[4];
    bool     _pressEdge[4];
    bool     _releaseEdge[4];

    State    _state;
    int8_t   _comboFirst;      // which button (0=A or 1=B) started the combo window
    uint32_t _comboFirstTime;  // when _comboFirst went stable-pressed
    uint32_t _comboBothTime;   // when both A+B were stable-pressed (0 = not yet)
    int8_t   _pendingAnswer;   // consumed by getAnswer()
    bool     _comboConfirmed;
};
