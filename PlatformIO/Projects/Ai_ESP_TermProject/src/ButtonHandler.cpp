#include "ButtonHandler.h"

ButtonHandler::ButtonHandler(const uint8_t pins[4]) {
    for (uint8_t i = 0; i < 4; i++) _pins[i] = pins[i];
}

void ButtonHandler::begin() {
    for (uint8_t i = 0; i < 4; i++) pinMode(_pins[i], INPUT_PULLUP);
    clearAll();
}

void ButtonHandler::clearAll() {
    for (uint8_t i = 0; i < 4; i++) {
        _raw[i] = false; _stable[i] = false;
        _debounceAt[i] = 0;
        _pressEdge[i] = false; _releaseEdge[i] = false;
    }
    _state          = WAITING;
    _comboFirst     = -1;
    _comboFirstTime = 0;
    _comboBothTime  = 0;
    _pendingAnswer  = -1;
    _comboConfirmed = false;
}

// ── update() ─────────────────────────────────────────────────────────────────
void ButtonHandler::update() {
    uint32_t now = millis();

    // Step 1 — debounce all buttons, compute press/release edges
    for (uint8_t i = 0; i < 4; i++) {
        _pressEdge[i] = _releaseEdge[i] = false;
        bool reading = (digitalRead(_pins[i]) == LOW);  // active-LOW

        if (reading != _raw[i]) { _raw[i] = reading; _debounceAt[i] = now; }

        if ((now - _debounceAt[i]) >= DEBOUNCE_MS && _stable[i] != _raw[i]) {
            bool prev = _stable[i];
            _stable[i] = _raw[i];
            if (!prev &&  _stable[i]) _pressEdge[i]   = true;
            if ( prev && !_stable[i]) _releaseEdge[i] = true;
        }
    }

    if (_state == ANSWER_LOCKED) return;  // all input suppressed

    // Step 2 — C (idx 2) and D (idx 3): immediate answer on press
    for (uint8_t i = 2; i <= 3; i++) {
        if (_pressEdge[i] && _state == WAITING) {
            _pendingAnswer = (int8_t)i;
            _state = ANSWER_LOCKED;
            return;
        }
    }

    // Step 3 — A/B combo state machine
    switch (_state) {

        case WAITING:
            if (_pressEdge[0] && _pressEdge[1]) {
                // Both A+B simultaneously — go straight to COMBO_ACTIVE
                _comboFirst = 0; _comboFirstTime = now;
                _comboBothTime = now;
                _state = COMBO_ACTIVE;
            } else if (_pressEdge[0]) {
                _comboFirst = 0; _comboFirstTime = now; _comboBothTime = 0;
                _state = COMBO_WINDOW;
            } else if (_pressEdge[1]) {
                _comboFirst = 1; _comboFirstTime = now; _comboBothTime = 0;
                _state = COMBO_WINDOW;
            }
            break;

        case COMBO_WINDOW: {
            uint8_t other = (_comboFirst == 0) ? 1 : 0;

            if (_pressEdge[other]) {
                // Second button joined → upgrade to COMBO_ACTIVE
                _comboBothTime = now;
                _state = COMBO_ACTIVE;
                break;
            }
            if (_releaseEdge[_comboFirst]) {
                // Solo tap: register as answer if short enough
                uint32_t dur = now - _comboFirstTime;
                if (dur < ANSWER_MAX_MS) {
                    _pendingAnswer = _comboFirst;
                    _state = ANSWER_LOCKED;
                } else {
                    _state = WAITING;
                }
            }
            // (if still held past 200ms with no combo, wait for release — handled above)
            break;
        }

        case COMBO_ACTIVE:
            // Confirm if both held long enough
            if (_comboBothTime > 0 && (now - _comboBothTime) >= COMBO_HOLD_MS) {
                _comboConfirmed = true;
                // Stay here; game logic calls clearAll() after handling
                break;
            }
            // One released before confirm
            if (_releaseEdge[0] || _releaseEdge[1]) {
                uint32_t dur = now - _comboFirstTime;
                if (dur < ANSWER_MAX_MS) {
                    _pendingAnswer = _comboFirst;
                    _state = ANSWER_LOCKED;
                } else {
                    _state = WAITING;
                }
                _comboConfirmed = false;
            }
            break;

        case ANSWER_LOCKED:
            break;
    }
}

// ── Accessors ─────────────────────────────────────────────────────────────────
int8_t ButtonHandler::getAnswer() {
    if (_pendingAnswer >= 0) { int8_t a = _pendingAnswer; _pendingAnswer = -1; return a; }
    return -1;
}

bool ButtonHandler::isComboConfirmed() { return _comboConfirmed; }

bool ButtonHandler::isComboHinting() {
    return (_state == COMBO_WINDOW || _state == COMBO_ACTIVE)
        && (millis() - _comboFirstTime) >= HINT_MS;
}

void ButtonHandler::unlock() {
    _state = WAITING;
    _pendingAnswer = -1;
    _comboConfirmed = false;
    for (uint8_t i = 0; i < 4; i++) { _pressEdge[i] = false; _releaseEdge[i] = false; }
}

// ── waitForAnyPress() — blocking, startup only ────────────────────────────────
int8_t ButtonHandler::waitForAnyPress() {
    unlock();
    bool anyHeld;
    do {
        anyHeld = false;
        update();
        for (uint8_t i = 0; i < 4; i++) if (_stable[i]) anyHeld = true;
        if (anyHeld) delay(20);
    } while (anyHeld);
    // Also break out if A+B combo confirmed (treat as "any press") to avoid deadlock
    while (true) {
        update();
        if (isComboConfirmed()) { unlock(); return 0; }
        int8_t a = getAnswer();
        if (a >= 0) { unlock(); return a; }
        delay(5);
    }
}
