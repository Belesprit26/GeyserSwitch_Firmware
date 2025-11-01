#pragma once

#include <Arduino.h>
#include "src/config/RtdbPaths.h"

// Abstracts the remote connectivity surface (cloud or BLE) for the Application.
// BLE may ignore RTDB paths; RTDB uses them to compose database URLs.
class RemoteBackend {
 public:
  virtual ~RemoteBackend() = default;

  // Initialize the backend. BLE may ignore paths.
  virtual void begin(const RtdbPaths* paths) = 0;

  // Progress background tasks (streams, advertising, etc.).
  virtual void loop() = 0;

  // Activate/deactivate the backend (subscribe/unsubscribe, start/stop advertising).
  virtual void activate(bool on) = 0;

  // Publish telemetry/state
  virtual bool publishTempC(float tempC) = 0;
  virtual bool publishRelayState(bool on) = 0;
  virtual bool publishLastUpdate(const String& hhmmss, const String& yyyymmdd) = 0;

  // Command subscription (invoked when a desired relay state is received)
  // C-style callback to avoid libstdc++ bloat from std::function
  using RelayCallback = void (*)(bool on, void* ctx);
  virtual void subscribeRelayCommand(RelayCallback onChange, void* ctx) = 0;

  // Settings ensure/get (create defaults if missing, then return current)
  virtual bool ensureMaxTemp(float defaultCelsius, float &outCelsius) = 0;
  virtual bool ensureHysteresis(float defaultCelsius, float &outCelsius) = 0;
  virtual bool ensureTimerFlag(const String &key, bool defaultEnabled, bool &outEnabled) = 0;
  virtual bool ensureCustomTime(const String &defaultHhmm, String &outHhmm) = 0;

  // Generic R/W for simple integer/string paths (e.g., usage totals)
  virtual bool setStringPath(const String &path, const String &value) = 0;
  virtual bool setIntPath(const String &path, int value) = 0;
  virtual bool getIntPath(const String &path, int &outValue) = 0;
};


