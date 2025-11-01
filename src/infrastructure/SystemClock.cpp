// SystemClock.cpp

#include "SystemClock.h"

#include <time.h>

void SystemClock::begin(const char* tz) {
  // Configure SNTP with given timezone string, e.g., "SAST-2" or full POSIX TZ.
  // Uses ESP32's built-in SNTP via configTzTime.
  configTzTime(tz, "pool.ntp.org", "time.nist.gov");
}

bool SystemClock::waitForTime(uint32_t timeoutMs) {
  const uint32_t start = millis();
  time_t t = time(nullptr);
  while ((t < 8 * 3600 * 2) && (millis() - start < timeoutMs)) {
    delay(50);
    t = time(nullptr);
  }
  return t >= 8 * 3600 * 2;  // arbitrary threshold: time set if > Jan 1, 1970
}

time_t SystemClock::now() const {
  return time(nullptr);
}


