#pragma once
#include <Arduino.h>
#include "TriviaQ.h"

class BackendClient {
public:
    BackendClient(const char* url);
    // Returns true on HTTP 200/201. Times out in 2s so it never stalls the game.
    bool post(const String& username, const TriviaQ& q,
              uint8_t pressedIdx, bool correct, uint8_t qNum);
private:
    const char* _url;
};
