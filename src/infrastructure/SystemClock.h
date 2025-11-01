// SystemClock.h
// Provides SNTP-backed time initialization and helpers.

#pragma once

#include <Arduino.h>

class SystemClock {
 public:
  // Initialize SNTP with timezone. Non-blocking; call waitForTime() to ensure sync.
  void begin(const char* tz);

  // Poll for time availability with timeoutMs. Returns true if time is set.
  bool waitForTime(uint32_t timeoutMs);

  // Returns current epoch seconds or 0 if not available.
  time_t now() const;
};


