#ifndef CONFIG_CACHE_H
#define CONFIG_CACHE_H

#include <Arduino.h>

// Centralized configuration cache refreshed periodically.
// Provides lightweight accessors for timers, custom time and max temperature.

// Initialize missing defaults once after auth is ready
void ensureDefaultTreeIfMissing();

// Refresh cached values at ~10s cadence (non-blocking)
void refreshConfigCache10s();

// Accessors (read from cache)
bool getTimerCached(int index);      // 0:04, 1:06, 2:08,3:16,4:18
String getCustomTimerCached();
double getMaxTemp1Cached();

#endif // CONFIG_CACHE_H


