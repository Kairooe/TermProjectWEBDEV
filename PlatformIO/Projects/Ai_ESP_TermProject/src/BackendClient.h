#pragma once
#include <Arduino.h>
#include "TriviaQ.h"

class BackendClient {
public:
    BackendClient(const char* baseUrl);
    // Posts one answer record to /api/device/record using the device code as auth.
    // Returns true on HTTP 201. Times out in 2s so it never stalls the game.
    bool post(const char* deviceCode, const TriviaQ& q,
              uint8_t pressedIdx, bool correct, uint8_t qNum,
              const String& subject);
private:
    const char* _baseUrl;
};
