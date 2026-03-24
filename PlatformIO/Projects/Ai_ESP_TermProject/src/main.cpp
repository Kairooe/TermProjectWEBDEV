#include <Arduino.h>
#include "WiFiManager.h"
#include "OLEDDisplay.h"
#include "ButtonHandler.h"
#include "LEDController.h"
#include "OllamaClient.h"

// ── Pin assignments  (A:btn19/led12  B:btn18/led13  C:btn5/led14  D:btn4/led15)
static const uint8_t BTN_PINS[4] = {19, 18,  5,  4};
static const uint8_t LED_PINS[4] = {12, 13, 14, 15};
static const char* const LABELS[4] = {"A", "B", "C", "D"};

// ── Ollama config ─────────────────────────────────────────────────────────────
static const char* OLLAMA_URL   = "http://192.168.1.166:11434/api/generate";
static const char* OLLAMA_MODEL = "qwen2.5:7b";
static const char* PROMPT =
    "Generate a trivia question with 4 multiple choice answers. "
    "Respond with ONLY a valid JSON object — no markdown, no explanation, nothing else. "
    "Exact format: {\"question\":\"...\",\"A\":\"...\",\"B\":\"...\",\"C\":\"...\",\"D\":\"...\",\"answer\":\"X\"} "
    "where answer is exactly one letter: A, B, C, or D. "
    "Keep the question under 60 characters. Keep each answer option under 18 characters.";

// ── Module instances ──────────────────────────────────────────────────────────
WiFiManager   wifiManager;
OLEDDisplay   oled;
ButtonHandler buttons(BTN_PINS);
LEDController leds(LED_PINS);
OllamaClient  ollama(OLLAMA_URL, OLLAMA_MODEL, PROMPT);

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("==============================");
    Serial.println("  ESP32 AI Trivia — Booting");
    Serial.println("==============================");

    buttons.begin();
    leds.begin();
    Serial.println("[PINS] Buttons: GPIO 19(A) 18(B)  5(C)  4(D)");
    Serial.println("[PINS] LEDs:    GPIO 12(A) 13(B) 14(C) 15(D)");

    if (!oled.begin(21, 22)) {
        Serial.println("[OLED] Fatal error — halting");
        while (true) delay(1000);
    }

    oled.showStatus("AI TRIVIA", "Connecting to", "WiFi...");
    wifiManager.begin();

    if (wifiManager.connected()) {
        oled.showStatus("AI TRIVIA", "WiFi connected!", wifiManager.ip().c_str());
        Serial.printf("[WIFI] IP: %s\n", wifiManager.ip().c_str());
    } else {
        oled.showStatus("AI TRIVIA", "WiFi failed.", "Continuing...");
    }
    delay(1500);

    oled.showStatus("AI TRIVIA", "Press any btn", "to start!");
    Serial.println("[READY] Waiting for button press...");
    buttons.waitForPress();
    Serial.println("[READY] Starting!");
}

// ── Main game loop ────────────────────────────────────────────────────────────
void loop() {
    static uint8_t qNum = 0;
    qNum++;

    Serial.printf("\n============================== Q%d\n", qNum);

    oled.showStatus("THINKING", "Asking AI...", OLLAMA_MODEL);
    TriviaQ q = ollama.fetch();

    if (!q.valid) {
        Serial.println("[ERROR] Failed to get valid question — retrying in 3s");
        oled.showStatus("ERROR", "AI fetch failed.", "Retrying...");
        delay(3000);
        qNum--;
        return;
    }

    oled.showQuestion(q, qNum);
    Serial.println("[GAME] Waiting for answer...");

    // For long questions: scroll the text left while polling for a button press.
    // maxScroll == 0 for short questions, so the loop reduces to a plain poll.
    const uint16_t qLen      = q.question.length();
    const uint16_t maxScroll = (qLen > 42) ? qLen - 42 : 0;
    uint16_t       scrollOff = 0;
    uint32_t       lastTick  = millis();
    int8_t         pressed   = -1;

    while (pressed < 0) {
        pressed = buttons.check();
        if (scrollOff < maxScroll && millis() - lastTick >= 200) {
            lastTick = millis();
            oled.showQuestion(q, qNum, ++scrollOff);
        }
    }
    bool   correct = (pressed == (int8_t)q.correct);

    Serial.printf("[GAME] Pressed : %s) %s\n", LABELS[pressed],   q.choices[pressed].c_str());
    Serial.printf("[GAME] Correct : %s) %s\n", LABELS[q.correct], q.choices[q.correct].c_str());
    Serial.printf("[GAME] Result  : %s\n",      correct ? "CORRECT ✓" : "WRONG ✗");

    oled.showFeedback(correct, (uint8_t)pressed, q);

    correct ? leds.flashCorrect((uint8_t)pressed)
            : leds.flashWrong(q.correct);

    leds.off();
    delay(1200);
}
