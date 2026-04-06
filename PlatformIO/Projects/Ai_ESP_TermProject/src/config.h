#pragma once

// ── Backend ───────────────────────────────────────────────────────────────────
#define BACKEND_BASE  "https://api.ktran.tech"

// ── Ollama AI ─────────────────────────────────────────────────────────────────
#define OLLAMA_URL      BACKEND_BASE "/api/ai/generate"
#define OLLAMA_MODEL    "qwen2.5:7b"

// ── WiFi captive-portal AP name (shown if no saved credentials) ───────────────
#define AP_NAME         "ESP32-Trivia-Setup"

// ── Preset usernames — cycle with A/B buttons, confirm with C or D ────────────
static const char* const PRESET_USERS[] = {
    "Player1", "Player2", "Player3", "Player4",
    "Alice",   "Bob",     "Charlie", "Diana"
};
static const int NUM_PRESET_USERS =
    (int)(sizeof(PRESET_USERS) / sizeof(PRESET_USERS[0]));
