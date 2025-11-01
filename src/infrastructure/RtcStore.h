// RtcStore.h
// Warm‑restart cache for last state/command using RTC memory (ESP32).
//
// Purpose
// - Provide very fast, volatile persistence across soft resets/brownouts without
//   writing flash, so the app can report the most recent relay state/intent quickly.
// - This is complementary to SettingsStore (flash): RTC data is lost on cold power loss.

#pragma once

#include <Arduino.h>
#include "src/infrastructure/SettingsStore.h"  // for PersistLastRecord

class RtcStore {
 public:
  // Load last record from RTC memory if valid; returns true when data present.
  static bool load(PersistLastRecord &out);
  // Save last record to RTC memory.
  static void save(const PersistLastRecord &in);
  // Clear RTC record so next boot won't use stale data.
  static void clear();
};


