// RtdbClientMobizt.h
// Wrapper around mobizt FirebaseClient for Realtime Database operations.
// Provides a small API we control for get/set/stream while keeping the rest of
// the app decoupled from library specifics.

#pragma once

#include <Arduino.h>

#include "src/config/BuildConfig.h"
#include "src/config/Secrets.h"
#include "src/config/RtdbPaths.h"
#include "src/infrastructure/Logger.h"
#include "src/infrastructure/RemoteBackend.h"


// Intentionally avoid including FirebaseClient headers here to keep the
// interface decoupled and minimize compile-time dependencies. Implementation
// includes the library in the .cpp.

class RtdbClientMobizt : public RemoteBackend {
 public:
  // Initialize the client with credentials from Secrets and the composed paths.
  // Safe to call once during app startup.
  void begin(const RtdbPaths* paths) override;

  // Processes any background tasks (e.g., stream handling). Call regularly.
  void loop() override;

  // Activate/deactivate backend (controls subscribing/stream processing).
  void activate(bool on) override;

  // RTDB health probe used by mode manager.
  bool isHealthy() const;

  // Publish helpers. Return true on success (when enabled), false otherwise.
  bool publishTempC(float tempC) override;
  bool publishRelayState(bool on) override;
  bool publishLastUpdate(const String& hhmmss, const String& yyyymmdd) override;

  // Subscribe to live relay state changes; callback invoked with desired state.
  void subscribeRelayCommand(RelayCallback onChange, void* ctx) override;


  // Pull (GET) helpers for settings (synchronous)
  bool getMaxTemp(float &outCelsius);
  bool getTimerFlag(const String &key, bool &outEnabled);
  bool getCustomTime(String &outHhmm);
  bool getHysteresis(float &outCelsius);

  // Ensure helpers: if missing, write default then return that value
  bool ensureMaxTemp(float defaultCelsius, float &outCelsius) override;
  bool ensureTimerFlag(const String &key, bool defaultEnabled, bool &outEnabled) override;
  bool ensureCustomTime(const String &defaultHhmm, String &outHhmm) override;
  bool ensureHysteresis(float defaultCelsius, float &outCelsius) override;

  // Generic path writers for app-side composite writes (usage records)
  bool setStringPath(const String &path, const String &value) override;
  bool setIntPath(const String &path, int value) override;
  bool getIntPath(const String &path, int &outValue) override;

 private:
  const RtdbPaths* paths_ = nullptr;
  RelayCallback relayCallback_ = nullptr;
  void* relayCtx_ = nullptr;
  bool active_ = true;

#if USE_MOBIZT_FIREBASE
  // Opaque impl to avoid exposing library types in the header
  void *impl_ = nullptr;
#endif
};


