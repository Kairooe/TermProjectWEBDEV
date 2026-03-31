#pragma once
#include <Arduino.h>

// ── DeviceClient ──────────────────────────────────────────────────────────────
// Wraps the two ESP32-facing backend endpoints:
//   POST /api/device/register  — create a pending session with a 6-digit code
//   GET  /api/device/poll/:code — check whether a web user has claimed it

class DeviceClient {
public:
    explicit DeviceClient(const char* baseUrl);

    // Register a 6-digit code with the backend.
    // Returns true on HTTP 201; false on any error or no WiFi.
    bool registerCode(const char* code);

    // Poll for claim status.
    // Returns:  1 = claimed (outUsername, outSubject, outDifficulty filled)
    //           0 = still pending
    //          -1 = session expired or not found (generate a new code)
    int8_t poll(const char* code, String& outUsername,
                String& outSubject, String& outDifficulty);

private:
    const char* _baseUrl;
};
