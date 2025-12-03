// Application.h
// High-level composition root for the GeyserSwitch firmware. Responsible for
// initializing subsystems (logging, configuration) and driving the main loop.

#pragma once

#include <Arduino.h>

#include "src/config/BuildConfig.h"
#include "src/config/Pins.h"
#include "src/config/Secrets.h"
#include "src/config/RtdbPaths.h"
#include "src/infrastructure/Logger.h"
#include "src/infrastructure/WifiManagerEsp32.h"
#include "src/infrastructure/SystemClock.h"
#include "src/infrastructure/DS18B20Sensor.h"
#include "src/infrastructure/GpioRelay.h"
#include "src/infrastructure/RemoteBackend.h"
#include "src/config/BuildConfig.h"
#if BUILD_ENABLE_RTDB
#include "src/infrastructure/RtdbClientMobizt.h"
#endif
#if BUILD_ENABLE_BLE
#include "src/infrastructure/ble/BleBackendNimble.h"
#endif

class Application {
 public:
  // Initializes logging and validates base configuration.
  // Safe to call only once from Arduino setup().
  void begin();

  // Runs a single iteration of the application's main loop. This function is
  // designed to be non-blocking and is called repeatedly from Arduino loop().
  void runLoop();

 private:
  bool initialized_ = false;  // Tracks whether begin() was called

  // Centralized RTDB path helper, rooted at basePath + "/" + userId.
  RtdbPaths rtdbPaths_;
  WifiManagerEsp32 wifi_;
  SystemClock clock_;
  DS18B20Sensor temp_;
  GpioRelay relay_;
#if BUILD_ENABLE_RTDB
  RtdbClientMobizt rtdb_;
#endif
#if BUILD_ENABLE_BLE
  BleBackendNimble ble_;
#endif
  RemoteBackend* remote_ = nullptr;  // active remote (RTDB now, BLE later)
  // Settings and command/state are kept in RAM only; no local flash/RTC persistence.
  // Last command seen via stream (for decision logs)
  bool lastCommandKnown_ = false;
  bool lastCommandOn_ = false;
  // Temperature smoothing/backoff
  bool haveSmoothedTemp_ = false;
  float smoothedTempC_ = 0.0f;
  int tempFailCount_ = 0;
  uint32_t nextTempReadAllowedMs_ = 0;

  // Settings cache (max temp and timers subset)
  struct SettingsCache {
    float maxTempC = 70.0f;  // default safety cutoff
    float hysteresisC = 2.0f; // default re-enable buffer
    String customTime;       // "HH:MM" for CUSTOM
    // Standard timer flags (enable windows starting at HH:MM)
    bool t0400 = false;
    bool t0600 = false;
    bool t0800 = false;
    bool t1600 = false;
    bool t1800 = false;
  } settings_;

  // Schedule trigger state (per day): prevents re-firing the same start
  // Bits: 0=04:00, 1=06:00, 2=08:00, 3=16:00, 4=18:00, 5=CUSTOM
  int lastScheduleYDay_ = -1;
  uint32_t scheduleFiredMask_ = 0;

  // Internal helpers
  void initializeLogger();
  void loadStaticConfig();  // loads basePath/userId from Secrets into rtdbPaths_
  void initializeWifiAndTime();
  void initializeCloud();
  void selectRemoteBackend();
  void initializeSensorsAndActuators();

  // Schedule helpers
  static int parseHhmmToMinutes(const String &hhmm);
  void processScheduleTriggers(bool haveTemp, float tempC);
  // Usage logging (remote only; no local persistence)
  void recordUsageOn(const char* reason, const char* instruction);
  void recordUsageOff(const char* reason, const char* instruction);
  String usageDayPath() const;
  String currentTimeStr() const;
  String currentDateStr() const;
  String openCycleId_ = String();
  uint32_t openCycleStartMs_ = 0;
  void addUsageToDailyTotal(uint32_t durationSec);
};


