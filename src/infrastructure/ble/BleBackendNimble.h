#pragma once

#include <Arduino.h>

#include "src/config/BuildConfig.h"
#include "src/infrastructure/RemoteBackend.h"
#include "src/infrastructure/SettingsStore.h"
#include "src/infrastructure/ble/BleUuids.h"

// BLE backend skeleton implementing RemoteBackend.
// This initial version does not start BLE yet; it provides stubs and
// local NVS-backed ensures for settings using SettingsStore.
class BleBackendNimble : public RemoteBackend {
 public:
  BleBackendNimble() = default;
  ~BleBackendNimble() override = default;

  void begin(const RtdbPaths* /*paths*/) override;
  void loop() override;
  void activate(bool on) override;

  bool publishTempC(float tempC) override;
  bool publishRelayState(bool on) override;
  bool publishLastUpdate(const String& hhmmss, const String& yyyymmdd) override;

  void subscribeRelayCommand(RelayCallback onChange, void* ctx) override;

  bool ensureMaxTemp(float defaultCelsius, float &outCelsius) override;
  bool ensureHysteresis(float defaultCelsius, float &outCelsius) override;
  bool ensureTimerFlag(const String &key, bool defaultEnabled, bool &outEnabled) override;
  bool ensureCustomTime(const String &defaultHhmm, String &outHhmm) override;

  bool setStringPath(const String &/*path*/, const String &/*value*/) override;
  bool setIntPath(const String &/*path*/, int /*value*/) override;
  bool getIntPath(const String &/*path*/, int &/*outValue*/) override;

 private:
  // Local persistence helpers
  bool loadSettings();
  bool saveSettings();

  // Timers bitmask helpers (for GATT write/read handlers)
  void setTimersBitmask(uint8_t mask, const String &defaultCustom = String("05:00"));
  uint8_t getTimersBitmask() const;

  SettingsStore store_;
  SettingsDto settings_{};
  PersistLastRecord last_{};
  bool loaded_ = false;
  bool active_ = false;
  RelayCallback relayCb_ = nullptr;
  void* relayCtx_ = nullptr;

  // Local-only hysteresis until persisted support is added
  float hysteresisC_ = 2.0f;

#if BUILD_ENABLE_BLE
  // NimBLE objects
  class CharWriteCb;  // defined in .cpp
  void updateCharacteristicMirrors();

  void initGatt();
  void startAdvertising();
  void stopAdvertising();

  // Characteristic handles
  void *server_ = nullptr;   // NimBLEServer*
  void *service_ = nullptr;  // NimBLEService*
  void *cCmd_ = nullptr;     // NimBLECharacteristic*
  void *cState_ = nullptr;
  void *cTempC_ = nullptr;
  void *cMaxC_ = nullptr;
  void *cHystC_ = nullptr;
  void *cTimers_ = nullptr;
  void *cCustom_ = nullptr;
  void *cTime_ = nullptr;
  void *cDate_ = nullptr;
  void *cTimeSync_ = nullptr;
  void *cUsageTotal_ = nullptr;
#endif
};


