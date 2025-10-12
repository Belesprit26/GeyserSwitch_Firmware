#pragma once

// Implementation of config cache kept in a header to allow inclusion from
// the sketch root for Arduino's build system, while keeping code organized
// under the interfaces/ directory.

#include "interfaces/config_cache.h"
#include "interfaces/firebase_functions.h"

namespace {
struct ConfigCache {
    bool timer04 = false;
    bool timer06 = false;
    bool timer08 = false;
    bool timer16 = false;
    bool timer18 = false;
    String customTimer;
    double maxTemp1 = 75.0;
    unsigned long lastRefreshMs = 0;
} cache;

void refreshInternal()
{
    if (!firebaseIsReady()) return;
        String gsFree = gsFree;
    cache.timer04 = dbGetBool(gsFree + String("/Timers/04:00"));
    cache.timer06 = dbGetBool(gsFree + String("/Timers/06:00"));
    cache.timer08 = dbGetBool(gsFree + String("/Timers/08:00"));
    cache.timer16 = dbGetBool(gsFree + String("/Timers/16:00"));
    cache.timer18 = dbGetBool(gsFree + String("/Timers/18:00"));
    cache.customTimer = dbGetString(gsFree + String("/Timers/CUSTOM"));
    cache.maxTemp1 = dbGetDouble(gsFree + String("/Geysers/geyser_1/max_temp"));
    if (dbLastErrorCode() != 0 || isnan(cache.maxTemp1) || cache.maxTemp1 <= 0.0) {
        cache.maxTemp1 = 75.0;
    }
}
} // namespace

void ensureDefaultTreeIfMissing()
{
    if (!firebaseIsReady()) return;
        String gsFree = gsFree;
    setDefaultBoolValue(gsFree + String("/Timers/04:00"), false);
    setDefaultBoolValue(gsFree + String("/Timers/06:00"), false);
    setDefaultBoolValue(gsFree + String("/Timers/08:00"), false);
    setDefaultBoolValue(gsFree + String("/Timers/16:00"), false);
    setDefaultBoolValue(gsFree + String("/Timers/18:00"), false);
    setDefaultStringValue(gsFree + String("/Timers/CUSTOM"), "");
    setDefaultBoolValue(gsFree + String("/Geysers/geyser_1/state"), false);
    setDefaultFloatValue(gsFree + String("/Geysers/geyser_1/sensor_1"), 204.00f);
    setDefaultDoubleValue(gsFree + String("/Geysers/geyser_1/max_temp"), 75.00);
}

void refreshConfigCache10s()
{
    if (!firebaseIsReady()) return;
    unsigned long nowMs = millis();
    if (nowMs - cache.lastRefreshMs >= 10000) {
        refreshInternal();
        cache.lastRefreshMs = nowMs;
    }
}

bool getTimerCached(int index)
{
    switch (index) {
        case 0: return cache.timer04;
        case 1: return cache.timer06;
        case 2: return cache.timer08;
        case 3: return cache.timer16;
        case 4: return cache.timer18;
        default: return false;
    }
}

String getCustomTimerCached()
{
    return cache.customTimer;
}

double getMaxTemp1Cached()
{
    return cache.maxTemp1;
}


