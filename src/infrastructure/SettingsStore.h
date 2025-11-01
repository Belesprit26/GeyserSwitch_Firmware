// SettingsStore.h
// Flash‑backed persistence for settings and last state/command using ESP32 NVS (Preferences).
//
// Purpose
// - Keep a durable copy of the most recent settings (target temp, timers, custom time)
//   and the last known ON/OFF state and command metadata across cold power cycles.
// - Application owns an in‑RAM cache (SettingsCache); this store converts to/from a
//   simple DTO structure (SettingsDto) and writes/reads compact keys in NVS.
// - We purposely avoid coupling to Application internals: the store depends only on
//   small POD‑like structs defined here.

#pragma once

#include <Arduino.h>

// Lightweight DTO for settings persisted to NVS.
// Application will map its `settings_` into this DTO when saving, and hydrate from it when loading.
struct SettingsDto {
  float maxTempC = 70.0f;
  String customTime;   // "HH:MM" or empty/false
  bool t0400 = false;
  bool t0600 = false;
  bool t0800 = false;
  bool t1600 = false;
  bool t1800 = false;
};

// Last edge/intent metadata stored alongside settings. This allows UI/logic to understand
// the most recent physical state (device truth) and the last remote intent, even after a reboot.
struct PersistLastRecord {
  bool hasLastState = false;
  bool lastStateOn = false;
  uint32_t lastStateTs = 0;  // millis or epoch if available
  bool hasLastCommand = false;
  bool lastCommandOn = false;
  uint32_t lastCommandTs = 0;
};

class SettingsStore {
 public:
  // Load settings and last record from flash (NVS namespace `geyserswitch`).
  // Returns true if a valid record was loaded (schema version present); otherwise leaves
  // the output values unchanged so the caller can retain defaults.
  bool load(SettingsDto &settingsOut, PersistLastRecord &lastOut);

  // Save settings and last record to flash. Returns true on success.
  // The caller should debounce and rate‑limit calls to avoid excessive flash wear.
  bool save(const SettingsDto &settingsIn, const PersistLastRecord &lastIn);

 private:
  static uint32_t computeCrc32(const uint8_t *data, size_t len);
};


