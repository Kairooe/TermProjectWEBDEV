#include <Arduino.h>
#include "wifi_manager.h"

// ── Game module includes ───────────────────────────────────────────────────────
// These are initialized after WiFi connects (see setup() below).
#include "config.h"
#include "OLEDDisplay.h"
#include "ButtonHandler.h"
#include "LEDController.h"
#include "OllamaClient.h"
#include "BackendClient.h"

// ===== PINS =====
#define PIN_BTN_A   19
#define PIN_BTN_B   18
#define PIN_BTN_C    5   // also used by WiFiManager for pairing trigger
#define PIN_BTN_D    4   // also used by WiFiManager for pairing trigger
#define PIN_LED_A   12
#define PIN_LED_B   13
#define PIN_LED_C   14
#define PIN_LED_D   15
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

// ── Ollama prompt ──────────────────────────────────────────────────────────────
#define OLLAMA_PROMPT \
    "Generate a trivia question with 4 multiple choice answers. " \
    "Respond with ONLY a valid JSON object — no markdown, no explanation, nothing else. " \
    "Exact format: {\"question\":\"...\",\"A\":\"...\",\"B\":\"...\",\"C\":\"...\",\"D\":\"...\",\"answer\":\"X\"} " \
    "where answer is exactly one letter: A, B, C, or D. " \
    "Keep the question under 60 characters. Keep each answer option under 18 characters."

// ===== TIMING =====
#define HIGHLIGHT_MS      300
#define FEEDBACK_MS      2500
#define SUMMARY_PAGE_MS  3000
#define WIFI_TIMEOUT_MS  5000
#define MAX_RETRIES          3

// ── U8g2 display instance ──────────────────────────────────────────────────────
// Passed to WiFiManager so it can draw provisioning screens.
// After wifiManager.begin() returns connected, OLEDDisplay re-inits
// the same hardware using the Adafruit SSD1306 driver for gameplay.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ── WiFi provisioning module ───────────────────────────────────────────────────
// Hold Button C (GPIO5) + Button D (GPIO4) for 3 s at any time to re-pair.
WiFiManager wifiManager(u8g2);

// ── Game module instances ──────────────────────────────────────────────────────
static const uint8_t BTN_PINS[4] = {PIN_BTN_A, PIN_BTN_B, PIN_BTN_C, PIN_BTN_D};
static const uint8_t LED_PINS[4] = {PIN_LED_A, PIN_LED_B, PIN_LED_C, PIN_LED_D};

OLEDDisplay   oled;
ButtonHandler buttons(BTN_PINS);
LEDController leds(LED_PINS);
OllamaClient  ollama(OLLAMA_URL, OLLAMA_MODEL, OLLAMA_PROMPT);
BackendClient backend(BACKEND_URL);

// ── Score / username tracking ─────────────────────────────────────────────────
static int    totalQuestions  = 0;
static int    correctAnswers  = 0;
static int    currentStreak   = 0;
static int    bestStreak      = 0;
static String currentUsername = "Player1";

// ── Game state ────────────────────────────────────────────────────────────────
enum GameState {
    GS_FETCHING,
    GS_QUESTION,
    GS_HIGHLIGHT,
    GS_FEEDBACK,
    GS_GAME_OVER,
};

static GameState gameState       = GS_FETCHING;
static TriviaQ   currentQ;
static int8_t    selectedAns     = -1;
static uint32_t  stateTimer      = 0;
static uint8_t   summaryPage     = 0;
static uint32_t  summaryTimer    = 0;
static uint8_t   lastSummaryPage = 255;

// ── Flag: game modules initialised after first successful WiFi connect ─────────
static bool gameInitialised = false;

// ── Forward declarations ──────────────────────────────────────────────────────
static void initGameModules();
static void handleInput();
static void handleDisplay();
static void handleNetwork();
static bool ensureWiFi();
static void enterGameOver();
static void resetGame();
static void pickUsername();

// ===== GAME MODULE INIT =======================================================
// Called once the first time isConnected() is true in loop().
// Separated from setup() because wifiManager.begin() may return before WiFi
// is connected (pairing portal mode) — game hardware must not init in that case.

static void initGameModules() {
    buttons.begin();
    leds.begin();

    // Re-initialise the SSD1306 with the Adafruit driver for game rendering.
    // WiFiManager used U8g2 on the same hardware; Adafruit's begin() takes over.
    if (!oled.begin(PIN_I2C_SDA, PIN_I2C_SCL)) {
        Serial.println("[OLED] Fatal — halting");
        while (true) delay(1000);
    }

    // NTP time sync for ISO-8601 timestamps in backend POSTs
    configTime(0, 0, "pool.ntp.org");
    struct tm tinfo;
    if (getLocalTime(&tinfo, 5000)) Serial.println("[TIME] NTP synced");
    else                            Serial.println("[TIME] NTP timeout — using epoch");

    // Username selection (blocking — uses buttons + oled)
    pickUsername();
    delay(300);

    // Press any button to start
    oled.showStatus("Digital Flashcard", "Press any button", "   to start!");
    Serial.println("[READY] Waiting for start...");
    buttons.waitForAnyPress();
    Serial.println("[READY] Starting!");

    gameState = GS_FETCHING;
}

// ===== NETWORK ================================================================

static bool ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) return true;
    oled.showStatus("WiFi Lost", "Reconnecting...", nullptr);
    Serial.println("[WIFI] Lost — reconnecting");
    WiFi.reconnect();
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
        delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) {
        oled.showStatus("WiFi OK", "Reconnected!", wifiManager.getIP().c_str());
        delay(800);
        return true;
    }
    oled.showStatus("WiFi Error", "Check router", nullptr);
    delay(2000);
    return false;
}

static void handleNetwork() {
    if (gameState != GS_FETCHING) return;
    if (!ensureWiFi()) return;

    oled.showStatus("THINKING", "Asking AI...", OLLAMA_MODEL);

    currentQ = TriviaQ{};
    for (uint8_t attempt = 0; attempt < MAX_RETRIES && !currentQ.valid; attempt++) {
        if (attempt > 0) {
            char buf[22];
            snprintf(buf, sizeof(buf), "Retry %d/%d...", attempt + 1, (int)MAX_RETRIES);
            oled.showStatus("API Timeout", buf, nullptr);
            delay(400);
            if (!ensureWiFi()) return;
        }
        currentQ = ollama.fetch();
    }

    if (!currentQ.valid) {
        oled.showStatus("Fetch Failed", "Check Ollama", nullptr);
        Serial.println("[NET] All retries failed");
        delay(3000);
        return;
    }

    totalQuestions++;
    Serial.printf("\n[NET] ===== Q%d =====\n", totalQuestions);

    leds.off();
    oled.loadQuestion(currentQ, (uint8_t)totalQuestions);
    buttons.unlock();
    gameState = GS_QUESTION;
}

// ===== INPUT ==================================================================

static void enterGameOver() {
    Serial.printf("[GAME] GAME OVER — Score:%d/%d  Best streak:%d\n",
                  correctAnswers, totalQuestions, bestStreak);
    buttons.clearAll();
    leds.flashGameOver();
    leds.off();
    delay(200);
    buttons.clearAll();
    gameState       = GS_GAME_OVER;
    summaryPage     = 0;
    lastSummaryPage = 255;
    summaryTimer    = millis();
}

static void handleInput() {
    buttons.update();

    switch (gameState) {

        case GS_QUESTION:
            oled.setComboHint(buttons.isComboHinting());
            if (buttons.isComboConfirmed()) { enterGameOver(); return; }
            selectedAns = buttons.getAnswer();
            if (selectedAns >= 0) {
                oled.setHighlight((uint8_t)selectedAns);
                stateTimer = millis();
                gameState  = GS_HIGHLIGHT;
            }
            break;

        case GS_FEEDBACK:
            if (buttons.isComboConfirmed()) { enterGameOver(); return; }
            break;

        case GS_GAME_OVER: {
            int8_t btn   = buttons.getAnswer();
            bool autoAdv = (millis() - summaryTimer >= SUMMARY_PAGE_MS);
            if (btn >= 0 || autoAdv) {
                if (summaryPage == 2 && btn >= 0) { resetGame(); return; }
                summaryPage  = (uint8_t)((summaryPage + 1) % 3);
                summaryTimer = millis();
                buttons.unlock();
            }
            break;
        }

        default: break;
    }
}

// ===== DISPLAY ================================================================

static void handleDisplay() {
    switch (gameState) {

        case GS_QUESTION:
        case GS_HIGHLIGHT:
        case GS_FEEDBACK:
            oled.tick();

            if (gameState == GS_HIGHLIGHT && millis() - stateTimer >= HIGHLIGHT_MS) {
                bool correct = (selectedAns == (int8_t)currentQ.correct);
                if (correct) {
                    correctAnswers++;
                    currentStreak++;
                    if (currentStreak > bestStreak) bestStreak = currentStreak;
                } else {
                    currentStreak = 0;
                }
                Serial.printf("[GAME] %s  pressed=%c  correct=%c  score=%d/%d  streak=%d\n",
                              correct ? "CORRECT" : "WRONG",
                              'A' + selectedAns, 'A' + currentQ.correct,
                              correctAnswers, totalQuestions, currentStreak);

                oled.clearHighlight();
                oled.setFeedback(correct, (uint8_t)selectedAns, currentQ);
                oled.tick();   // force display update NOW, before blocking LED flash + HTTP
                leds.off();
                correct ? leds.flashCorrect((uint8_t)selectedAns)
                        : leds.flashWrong(currentQ.correct);
                leds.off();

                // backend.post() disabled until server is running at BACKEND_URL
                // bool saved = backend.post(currentUsername, currentQ,
                //                           (uint8_t)selectedAns, correct,
                //                           (uint8_t)totalQuestions);
                // if (saved) oled.setFeedbackNote("Saved!");

                stateTimer = millis();
                gameState  = GS_FEEDBACK;
            }

            if (gameState == GS_FEEDBACK && millis() - stateTimer >= FEEDBACK_MS) {
                gameState = GS_FETCHING;
            }
            break;

        case GS_GAME_OVER:
            if (lastSummaryPage != summaryPage) {
                lastSummaryPage = summaryPage;
                switch (summaryPage) {
                    case 0: oled.showSummaryScore(totalQuestions, correctAnswers);  break;
                    case 1: oled.showSummaryStreak(bestStreak);                      break;
                    case 2: oled.showSummaryRestart();                               break;
                }
            }
            break;

        default: break;
    }
}

// ===== GAME LOGIC =============================================================

static void resetGame() {
    totalQuestions  = 0;
    correctAnswers  = 0;
    currentStreak   = 0;
    bestStreak      = 0;
    selectedAns     = -1;
    lastSummaryPage = 255;
    buttons.clearAll();
    gameState = GS_FETCHING;
    Serial.println("[GAME] --- RESTART ---");
}

// ===== USERNAME PICKER ========================================================
// A = previous, B = next, C or D = confirm.

static void pickUsername() {
    Preferences prefs;
    prefs.begin("trivia", false);
    uint8_t idx = prefs.getUChar("user", 0);
    if (idx >= NUM_PRESET_USERS) idx = 0;

    buttons.clearAll();

    auto refresh = [&]() {
        char hint[22];
        snprintf(hint, sizeof(hint), "A< B>  C/D OK (%d/%d)",
                 (int)(idx + 1), (int)NUM_PRESET_USERS);
        oled.showStatus("Who's playing?", PRESET_USERS[idx], hint);
    };
    refresh();

    while (true) {
        buttons.update();
        int8_t btn = buttons.getAnswer();
        if (btn == 0) {
            idx = (idx + NUM_PRESET_USERS - 1) % NUM_PRESET_USERS;
            refresh();
            buttons.unlock();
        } else if (btn == 1) {
            idx = (idx + 1) % NUM_PRESET_USERS;
            refresh();
            buttons.unlock();
        } else if (btn >= 2) {
            break;
        }
        delay(10);
    }

    prefs.putUChar("user", idx);
    prefs.end();
    currentUsername = String(PRESET_USERS[idx]);
    Serial.printf("[USER] Selected: %s\n", currentUsername.c_str());
}

// ===== SETUP ==================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n==============================");
    Serial.println("  TriviaGame — Booting");
    Serial.println("==============================");

    // ── WiFi provisioning ──────────────────────────────────────────────────────
    // Handles auto-connect from NVS or opens captive-portal AP (async, non-blocking).
    // Hold Button C + Button D before power-on to force re-pairing.
    // If connecting takes up to 30 s, this call blocks for that duration.
    // In pairing mode the call returns immediately; the portal runs in the background.
    wifiManager.begin();

    // ── Game modules are initialised in loop() on first connected iteration ───
    // (see initGameModules() / gameInitialised flag)
}

// ===== LOOP ===================================================================

void loop() {
    // ── WiFiManager must tick every iteration ─────────────────────────────────
    // Handles: C+D 3-second re-pair detection, deferred ESP.restart() after
    // a successful /connect form submission.
    wifiManager.loop();

    // ── Gate all game logic behind an active WiFi connection ─────────────────
    // While in pairing mode this returns immediately each iteration, keeping
    // the async web server and DNS redirector responsive.
    if (!wifiManager.isConnected()) return;

    // ── One-time game module initialisation ───────────────────────────────────
    // Runs only on the first loop() iteration where WiFi is confirmed connected,
    // which may be the boot after a successful /connect + ESP.restart().
    if (!gameInitialised) {
        gameInitialised = true;
        initGameModules();
    }

    // ── Game logic ─────────────────────────────────────────────────────────────
    handleInput();
    handleDisplay();
    handleNetwork();
}
