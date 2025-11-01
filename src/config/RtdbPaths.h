// RtdbPaths.h
// Centralized helpers to build RTDB paths rooted at basePath + "/" + userId.

#pragma once

#include <Arduino.h>

struct RtdbPaths {
  String basePath;   // e.g. "/GeyserSwitch" (no trailing '/')
  String userId;     // provisioned; temp from Secrets (non-empty)

  // Ensures all paths start at root = basePath + "/" + userId.
  String root() const {
    String bp = basePath;
    if (bp.length() == 0 || bp[0] != '/') bp = "/" + bp;  // ensure leading slash
    if (bp.endsWith("/")) bp.remove(bp.length() - 1);      // drop trailing slash
    return bp + "/" + userId;
  }

  // Timers
  String timersRoot() const { return root() + F("/Timers"); }
  String timerKey(const String &hhmm) const { return timersRoot() + F("/") + hhmm; }

  // Geyser
  String geyserState() const { return root() + F("/Geysers/geyser_1/state"); }
  String hysteresisC() const { return root() + F("/Geysers/geyser_1/hysteresis_c"); }
  // Remote control command path (device listens here)
  String geyserCommand() const { return root() + F("/Geysers/geyser_1/command"); }

  // Sensor
  String sensorTemp() const { return root() + F("/Geysers/geyser_1/sensor_1"); }
  String maxTemp() const { return root() + F("/Geysers/geyser_1/max_temp"); }

  // Records
  String usageDay(const String &isoDate) const { return root() + F("/Records/GeyserUsage/") + isoDate; }
  String lastUpdateTime() const { return root() + F("/Records/LastUpdate/updateTime"); }
  String lastUpdateDate() const { return root() + F("/Records/LastUpdate/updateDate"); }
};


