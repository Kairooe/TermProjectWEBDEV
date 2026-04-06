/**
 * wifi_manager.h
 *
 * WiFi provisioning and connection management for the ESP32 Trivia Game.
 *
 * Responsibilities:
 *   - Boot-time auto-connect using NVS-stored credentials (Preferences)
 *   - Captive-portal pairing mode via SoftAP + AsyncWebServer + AsyncDNSServer
 *   - Pairing mode trigger: hold Button C (GPIO5) + Button D (GPIO4) for 3 s
 *     at boot OR at any point during gameplay (non-blocking, millis()-based)
 *   - Supports WPA/WPA2-Personal, Open, Open+Captive-Portal, WPA2-Enterprise (PEAP)
 *   - Generates a fresh random 8-digit PIN per pairing session — never persisted
 *   - OLED feedback via U8g2
 */

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncDNSServer.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <esp_wpa2.h>

// ── Pin definitions ────────────────────────────────────────────────────────────
#define WM_PIN_BTN_C   5    // Button C — active LOW, INPUT_PULLUP
#define WM_PIN_BTN_D   4    // Button D — active LOW, INPUT_PULLUP
#define WM_PIN_SDA    21
#define WM_PIN_SCL    22

// ── SoftAP identity ────────────────────────────────────────────────────────────
#define WM_AP_SSID  "TriviaGame-Setup"
#define WM_AP_IP    "192.168.4.1"

// ── NVS storage ────────────────────────────────────────────────────────────────
#define WM_NVS_NAMESPACE  "wifi_cfg"
#define WM_NVS_KEY_SSID   "ssid"
#define WM_NVS_KEY_PASS   "password"
#define WM_NVS_KEY_IDENT  "identity"
#define WM_NVS_KEY_AUTH   "auth_type"

// ── Timing ─────────────────────────────────────────────────────────────────────
#define WM_HOLD_DURATION        3000UL   // ms C+D must be held to trigger pairing
#define WM_CONNECT_TIMEOUT_MS  900000UL  // ms timeout for /connect form submission (15 min)
#define WM_AUTOCONNECT_TIMEOUT 10000UL   // ms timeout for boot auto-connect

class WiFiManager {
public:
    // ── Constructor ───────────────────────────────────────────────────────────
    // Pass the U8G2 display instance created in main (already begun).
    explicit WiFiManager(U8G2 &display);

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void   begin();   // Call once in setup() — handles boot auto-connect or pairing
    void   loop();    // Call every iteration of loop() — non-blocking C+D detection

    // ── State queries ─────────────────────────────────────────────────────────
    bool   isConnected() const;
    bool   isPairing()   const;
    String getIP()       const;

private:
    // ── Hardware + libraries ──────────────────────────────────────────────────
    U8G2          &_display;
    Preferences    _prefs;
    AsyncWebServer _server;   // AsyncWebServer(80) — initialised in constructor
    AsyncDNSServer _dns;

    // ── Runtime state ─────────────────────────────────────────────────────────
    bool _connected;
    bool _pairingMode;
    char _generatedPin[9];    // 8-digit numeric string + null — RAM only, never NVS

    // ── C+D hold detection (non-blocking) ─────────────────────────────────────
    unsigned long _holdStart;   // millis() snapshot of when the hold began
    bool          _holdActive;  // true while both buttons remain continuously held

    // ── Cached NVS credentials ────────────────────────────────────────────────
    String _savedSSID;
    String _savedPassword;
    String _savedIdentity;
    String _savedAuthType;   // "open" | "personal" | "enterprise"

    // ── PIN generation ────────────────────────────────────────────────────────
    // Fills _generatedPin with a fresh 8-digit string using esp_random().
    void generatePin();

    // ── Pairing mode lifecycle ────────────────────────────────────────────────
    void startPairingMode();   // Erases NVS, starts softAP, server, DNS
    void stopPairingMode();    // Tears down server, DNS, softAP

    // ── Connection helpers ────────────────────────────────────────────────────
    // Dispatches to the correct connect path and polls until connected or timeout.
    bool attemptConnect(const String &ssid, const String &password,
                        const String &identity, const String &authType,
                        unsigned long timeoutMs = WM_CONNECT_TIMEOUT_MS);

    // WPA2-Enterprise (PEAP/MSCHAPv2) path via esp_wpa2 API.
    bool connectEnterprise(const String &ssid, const String &identity,
                           const String &password);

    // ── NVS helpers ───────────────────────────────────────────────────────────
    void saveCredentials(const String &ssid, const String &password,
                         const String &identity, const String &authType);
    bool loadCredentials();   // Returns true if a saved SSID was found
    void eraseCredentials();  // Calls preferences.clear() on WM_NVS_NAMESPACE

    // ── Web server ────────────────────────────────────────────────────────────
    // Registers: GET /, GET /scan, POST /connect, and captive-portal detection
    // endpoints (generate_204, hotspot-detect.html, fwlink).
    void registerRoutes();

    // ── OLED helpers ──────────────────────────────────────────────────────────
    // Draws the full pairing screen: AP SSID, PIN (large font), IP address.
    void oledPairingScreen();

    // Draws up to three status lines using the normal 6×10 font.
    void oledStatus(const String &line1,
                    const String &line2 = "",
                    const String &line3 = "");

    // ── Button helper ─────────────────────────────────────────────────────────
    // Returns true if both GPIO5 (C) and GPIO4 (D) read LOW simultaneously.
    bool bothButtonsHeld() const;
};
