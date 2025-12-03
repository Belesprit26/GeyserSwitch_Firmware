// Application.cpp
// See Application.h for high-level responsibilities.

#include "Application.h"
#include <time.h>
#include "src/domain/ControlPolicy.h"

void Application::begin() {
  if (initialized_) return;

  initializeLogger();

  // Load compile- or secrets-provided configuration and derive RTDB root.
  loadStaticConfig();

  initializeWifiAndTime();

  initializeCloud();

  initializeSensorsAndActuators();

  Logger::info("Application initialized. Root path: %s", rtdbPaths_.root().c_str());
  initialized_ = true;
}

void Application::runLoop() {
  if (!initialized_) return;

  // Placeholder for future task processing. Keep it fast and non-blocking.
  // We'll add cooperative polling here until FreeRTOS tasks are wired.
  wifi_.ensureConnected();
  // Maintain active remote backend (auto switch per connectivity)
  selectRemoteBackend();
  if (remote_) remote_->loop();

  // Periodic control + temperature logging every 10s.
  static uint32_t lastTempLogMs = 0;
  static uint32_t lastLastUpdateMs = 0;
  const uint32_t nowMs = millis();
  if (nowMs - lastTempLogMs >= 15000u) {
    lastTempLogMs = nowMs;
    // Pull/ensure settings once per 10s cycle (simple periodic GETs)
    float mt = settings_.maxTempC;
    if (!remote_ || !remote_->ensureMaxTemp(settings_.maxTempC, mt)) {
      Logger::warn("Settings: ensure max_temp failed");
    } else {
      settings_.maxTempC = mt;
    }
    float hy = settings_.hysteresisC;
    if (!remote_ || !remote_->ensureHysteresis(settings_.hysteresisC, hy)) {
      Logger::warn("Settings: ensure hysteresis failed");
    } else {
      settings_.hysteresisC = hy;
    }
    String custom;
    if (!remote_ || !remote_->ensureCustomTime(settings_.customTime.length() ? settings_.customTime : String("05:00"), custom)) {
      Logger::warn("Settings: ensure CUSTOM failed");
    } else {
      settings_.customTime = custom;
    }
    if (remote_) {
      remote_->ensureTimerFlag("04:00", false, settings_.t0400);
      remote_->ensureTimerFlag("06:00", false, settings_.t0600);
      remote_->ensureTimerFlag("08:00", false, settings_.t0800);
      remote_->ensureTimerFlag("16:00", false, settings_.t1600);
      remote_->ensureTimerFlag("18:00", false, settings_.t1800);
    }
#if BUILD_LOG_SETTINGS_VERBOSE
    Logger::warn(
      "Timers: { 04:00: %s, 06:00: %s, 08:00: %s, 16:00: %s, 18:00: %s, CUSTOM: %s }",
      settings_.t0400 ? "true" : "false",
      settings_.t0600 ? "true" : "false",
      settings_.t0800 ? "true" : "false",
      settings_.t1600 ? "true" : "false",
      settings_.t1800 ? "true" : "false",
      settings_.customTime.c_str()
    );
    Logger::warn("Max-T: Target Temperature = %.0f'C (hyst=%.1fC)", settings_.maxTempC, settings_.hysteresisC);
#endif
    // DS18B20 smoothing + failure backoff
    // Strategy:
    // 1) Only attempt a sensor read if we are past `nextTempReadAllowedMs_` (backoff gate).
    // 2) On read failure, increment `tempFailCount_` and exponentially increase the backoff
    //    before the next read attempt (capped at 60 seconds). This avoids hammering the bus
    //    when the sensor is absent or wiring is faulty.
    // 3) On read success, reset the failure counter and update an Exponential Moving Average (EMA)
    //    into `smoothedTempC_` using alpha=0.3 (first sample seeds the EMA). We publish the raw
    //    reading to RTDB for transparency but use the smoothed value for control decisions to
    //    reduce jitter.
    // 4) If in a backoff window and we already have a smoothed value, we reuse it; otherwise,
    //    we log that no temperature is currently available.
    float tC = 0.0f;
    bool haveTemp = false;
    if (nowMs >= nextTempReadAllowedMs_) {
      // Attempt a fresh read from the sensor since backoff gate is open
      haveTemp = temp_.readCelsius(tC);
      if (!haveTemp) {
        // Failure: increase the failure count and compute next backoff
        // Base backoff is 1s and doubles each failure (1,2,4,8,16,32,64),
        // capped to 60,000 ms. The `min(tempFailCount_, 6)` caps the power-of-two
        // at 2^6 = 64s, and the outer min() clamps it to 60s hard.
        tempFailCount_++;
        uint32_t backoff = min<uint32_t>(60000u, (uint32_t)(1000u * (1u << min(tempFailCount_, 6))));
        nextTempReadAllowedMs_ = nowMs + backoff;
        Logger::warn("Temp: device not found or read failed (fail=%d, backoff=%ums)", tempFailCount_, backoff);
      } else {
        // Success: reset failure/backoff state
        tempFailCount_ = 0;
        nextTempReadAllowedMs_ = nowMs;
        // Update EMA smoothing: first sample seeds the average; subsequent samples blend
        // using alpha=0.3 (70% of the previous average retained).
        if (!haveSmoothedTemp_) {
          smoothedTempC_ = tC;       // seed EMA on first successful read
          haveSmoothedTemp_ = true;
        } else {
          const float alpha = 0.3f;  // smoothing factor; lower = smoother, slower to react
          smoothedTempC_ = alpha * tC + (1.0f - alpha) * smoothedTempC_;
        }
        // Publish raw reading for observability; control below uses `smoothedTempC_`.
        Logger::info("Temp: %.2f C (smoothed=%.2f)", tC, smoothedTempC_);
        if (remote_) remote_->publishTempC(tC);
      }
    } else {
      // We are currently in a backoff window; skip hitting the sensor bus.
      // If we have a previously smoothed value, reuse it for control decisions;
      // otherwise we log lack of data and control will treat temp as unavailable.
      haveTemp = haveSmoothedTemp_;
      if (!haveTemp) Logger::warn("Temp: backoff active and no prior value");
    }

    // Fire schedule triggers at exact times (start-only), then safety will auto-OFF at maxTemp
    processScheduleTriggers(haveTemp, tC);
    bool scheduleActive = false; // triggers now manage ON; leave false here

    // Control evaluation (command handled via stream for ON decisions)
    ControlInputs ci{};
    ci.hasCommand = false;        // command stream sets relay directly; no latched command cache yet
    ci.commandOn = false;
    ci.scheduleActive = scheduleActive;
    // For control, prefer the smoothed temperature (if available) to avoid
    // rapid toggling near thresholds. If not available, use 0.0 which is
    // interpreted alongside `haveTemp` checks below.
    ci.tempC = haveTemp ? smoothedTempC_ : 0.0f;
    ci.maxTempC = settings_.maxTempC;
    ci.hysteresisC = settings_.hysteresisC;
    ci.relayCurrentlyOn = relay_.isOn();

    ControlDecision cd = ControlPolicy::evaluate(ci);
    // Only apply control if it would turn OFF due to safety; ON decisions are left to command/schedule
    // Enforce safety cutoff only when we actually have a valid temperature reading.
    if (haveTemp && ci.tempC >= ci.maxTempC && relay_.isOn()) {
      relay_.setOn(false);
      Logger::warn("Control: target temperature cutoff at %.2f >= %.2f -> OFF", ci.tempC, ci.maxTempC);
      if (remote_) remote_->publishRelayState(false);
      recordUsageOff("targetTemp", "fromDevice");
    }

    // Concise control decision log
    const char* cmdStr = lastCommandKnown_ ? (lastCommandOn_ ? "ON" : "OFF") : "n/a";
    Logger::info(
      "Decision: cmd=%s, sched=%s, temp=%.1fC, hyst=%.1fC, state=%s",
      cmdStr,
      (scheduleActive ? "ON" : "OFF"),
      ci.tempC,
      settings_.hysteresisC,
      relay_.isOn() ? "ON" : "OFF"
    );
  }

  // Periodic LastUpdate write (time/date) every 10s, rate-limited
  if (nowMs - lastLastUpdateMs >= 15000u) {
    lastLastUpdateMs = nowMs;
    time_t nowSec = clock_.now();
    if (nowSec > 0) {
      struct tm *lt = localtime(&nowSec);
      if (lt) {
        char timeBuf[9];   // HH:MM:SS
        char dateBuf[11];  // YYYY-MM-DD
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", lt);
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", lt);
        if (remote_) remote_->publishLastUpdate(String(timeBuf), String(dateBuf));
      }
    }
  }

  delay(5);  // Yield briefly to avoid tight spin in early skeleton
}

String Application::currentTimeStr() const {
  time_t nowSec = time(nullptr);
  struct tm *lt = localtime(&nowSec);
  if (!lt) return String("00:00");
  char buf[9];
  strftime(buf, sizeof(buf), "%H:%M", lt);
  return String(buf);
}

String Application::currentDateStr() const {
  time_t nowSec = time(nullptr);
  struct tm *lt = localtime(&nowSec);
  if (!lt) return String("1970-01-01");
  char buf[11];
  strftime(buf, sizeof(buf), "%Y-%m-%d", lt);
  return String(buf);
}

String Application::usageDayPath() const {
  return rtdbPaths_.usageDay(currentDateStr());
}

void Application::recordUsageOn(const char* reason, const char* instruction) {
  String pushId = String("cy_") + String(millis());
  openCycleId_ = pushId;
  openCycleStartMs_ = millis();
  String base = usageDayPath() + "/cycles/" + pushId + "/";
  if (remote_) {
    remote_->setStringPath(base + "startTime", currentTimeStr());
    remote_->setStringPath(base + "startReason", String(reason));
    remote_->setStringPath(base + "startInstruction", String(instruction));
  }
}

void Application::recordUsageOff(const char* reason, const char* instruction) {
  if (openCycleId_.length() == 0) return;
  uint32_t dur = 0;
  if (openCycleStartMs_ != 0) dur = (millis() - openCycleStartMs_) / 1000u;
  String base = usageDayPath() + "/cycles/" + openCycleId_ + "/";
  if (remote_) {
    remote_->setStringPath(base + "endTime", currentTimeStr());
    remote_->setStringPath(base + "endReason", String(reason));
    remote_->setStringPath(base + "endInstruction", String(instruction));
    remote_->setIntPath(base + "durationSec", (int)dur);
  }
  addUsageToDailyTotal(dur);
  openCycleId_.remove(0);
  openCycleStartMs_ = 0;
}

void Application::addUsageToDailyTotal(uint32_t durationSec) {
  // read-modify-write totalDurationSec for the day
  String dayPath = usageDayPath();
  int total = 0;
  if (!remote_ || !remote_->getIntPath(dayPath + "/totalDurationSec", total)) {
    total = 0;  // assume missing
  }
  total += (int)durationSec;
  if (remote_) remote_->setIntPath(dayPath + "/totalDurationSec", total);
}

void Application::initializeLogger() {
  // Initialize Serial with a reasonable baud rate for logs.
  Serial.begin(BUILD_LOG_BAUD_RATE);
  while (!Serial) {
    // Wait briefly for Serial on boards that require it; timeout to avoid boot stalls.
    delay(10);
  }
  Logger::setLevel(LOG_LEVEL_INFO);
  Logger::info("Logger initialized (baud=%d)", BUILD_LOG_BAUD_RATE);
}

void Application::loadStaticConfig() {
  // Ensure basePath and userId are provided by Secrets.h and normalize them.
  rtdbPaths_.basePath = SECRETS_BASE_PATH;  // e.g. "/GeyserSwitch"
  rtdbPaths_.userId = SECRETS_USER_ID;      // provisioned later
}

void Application::initializeWifiAndTime() {
  // Start Wi-Fi connect sequence with exponential backoff.
  wifi_.begin(SECRETS_WIFI_SSID, SECRETS_WIFI_PASSWORD);

  // Non-blocking: opportunistically wait a short time to get initial connection.
  const uint32_t softWaitMs = 3000;  // short bootstrap window
  uint32_t start = millis();
  while (millis() - start < softWaitMs) {
    if (wifi_.ensureConnected()) break;
    delay(50);
  }
  if (wifi_.isConnected()) {
    Logger::info("WiFi up: IP=%s RSSI=%d", wifi_.localIp().toString().c_str(), wifi_.getRssi());
  } else {
    Logger::warn("WiFi not connected yet; continuing with background retries");
  }

  // Initialize SNTP time (South Africa Standard Time example: SAST-2)
  clock_.begin("SAST-2");
}

void Application::initializeSensorsAndActuators() {
  // Initialize DS18B20 (may not be connected yet; begin() will warn if none).
  temp_.begin(PIN_DS18B20_DATA);

  // Initialize relay/LED on GPIO defined in Pins.h.
  // Typical relay modules are active-LOW; onboard LEDs are usually active-HIGH.
  bool activeLow = false;
  if (PIN_RELAY_CTRL == 15) {
    activeLow = false;  // onboard LED on GPIO 15 is active-HIGH
  }
  relay_ = GpioRelay(activeLow);
  relay_.begin(PIN_RELAY_CTRL);
}

int Application::parseHhmmToMinutes(const String &hhmm) {
  if (hhmm.length() != 5 || hhmm.charAt(2) != ':') return -1;
  int hh = hhmm.substring(0,2).toInt();
  int mm = hhmm.substring(3,5).toInt();
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return -1;
  return hh * 60 + mm;
}

// legacy window checker removed; triggers manage ON behavior

void Application::processScheduleTriggers(bool haveTemp, float tempC) {
  // Reset fired mask on new day
  time_t nowSec = time(nullptr);
  struct tm *lt = localtime(&nowSec);
  if (!lt) return;
  if (lastScheduleYDay_ != lt->tm_yday) {
    lastScheduleYDay_ = lt->tm_yday;
    scheduleFiredMask_ = 0;
  }

  auto maybeFire = [&](int bit, const char* hhmmFlag, bool enabled){
    if (!enabled) return;
    int startMin = parseHhmmToMinutes(String(hhmmFlag));
    if (startMin < 0) return;
    int nowMin = lt->tm_hour * 60 + lt->tm_min;
    // Allow a 0..59s window since our loop ticks every ~10s; match on minute only
    if (nowMin == startMin) {
      if ((scheduleFiredMask_ & (1u << bit)) == 0) {
        // Fire ON if below re-enable threshold
        float reenable = settings_.maxTempC - settings_.hysteresisC; // hysteresis
        if (!haveTemp || tempC < reenable) {
          relay_.setOn(true);
          if (haveTemp) {
            Logger::info("Schedule: trigger %s -> ON (temp=%.1f < %.1f)", hhmmFlag, tempC, reenable);
          } else {
            Logger::info("Schedule: trigger %s -> ON (no temp yet)", hhmmFlag);
          }
          if (remote_) remote_->publishRelayState(true);
          recordUsageOn("schedule", "fromDevice");
        } else {
          Logger::info("Schedule: trigger %s skipped (temp=%.1f >= %.1f)", hhmmFlag, tempC, reenable);
        }
        scheduleFiredMask_ |= (1u << bit);
      }
    }
  };

  maybeFire(0, "04:00", settings_.t0400);
  maybeFire(1, "06:00", settings_.t0600);
  maybeFire(2, "08:00", settings_.t0800);
  maybeFire(3, "16:00", settings_.t1600);
  maybeFire(4, "18:00", settings_.t1800);
  if (settings_.customTime.length() == 5) {
    maybeFire(5, settings_.customTime.c_str(), true);
  }
}

void Application::initializeCloud() {
  // Initialize RTDB client and subscribe to live relay commands. When a command
  // comes in, toggle relay immediately.
  #if BUILD_ENABLE_RTDB
  rtdb_.begin(&rtdbPaths_);
  #endif
  #if BUILD_ENABLE_BLE
  ble_.begin(&rtdbPaths_);
  #endif
  // Choose backend per flavor
  #if BUILD_ENABLE_RTDB
  remote_ = &rtdb_;
  #elif BUILD_ENABLE_BLE
  remote_ = &ble_;
  #else
  remote_ = nullptr;
  #endif
  struct RelayThunk { static void call(bool on, void* ctx) {
    Application* self = static_cast<Application*>(ctx);
    if (!self) return;
    // Map Firebase boolean directly to hardware state:
    // true -> pin HIGH (LED ON when active-high), false -> pin LOW (LED OFF)
    bool hwOn = on;
    bool wasOn = self->relay_.isOn();
    self->relay_.setOn(hwOn);
    Logger::info("Relay set %s via RTDB", hwOn ? "ON" : "OFF");
    // Do NOT write back to the same path here; that would create a feedback loop
    // where our write triggers the stream again and flips repeatedly.
    // Mirror physical state to the state path so remote clients can see the device result
    if (self->remote_) self->remote_->publishRelayState(hwOn);
    // Track for decision logs
    self->lastCommandKnown_ = true;
    self->lastCommandOn_ = on;
    // Only record usage when the physical state actually changes
    if (hwOn != wasOn) {
      if (hwOn) self->recordUsageOn("command", "fromUser");
      else self->recordUsageOff("command", "fromUser");
    }
  }};
  #if BUILD_ENABLE_RTDB
  rtdb_.subscribeRelayCommand(&RelayThunk::call, this);
  #endif
  #if BUILD_ENABLE_BLE
  ble_.subscribeRelayCommand(&RelayThunk::call, this);
  #endif

  // Settings subscriptions removed; we use pull-only ensure in runLoop()

  // Activate the selected backend so it can start processing (RTDB auth loop or BLE advertising).
  if (remote_) {
    remote_->activate(true);
  }
}

void Application::selectRemoteBackend() {
  // Compile-time flavor: backend chosen at init, no switching
}

