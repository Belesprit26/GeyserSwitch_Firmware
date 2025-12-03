#include "BleBackendNimble.h"

#include "src/infrastructure/Logger.h"
#include "src/config/BuildConfig.h"

#if BUILD_ENABLE_BLE
#include <sys/time.h>
#include <NimBLEDevice.h>
#include <algorithm>
#endif

void BleBackendNimble::begin(const RtdbPaths* /*paths*/) {
#if BUILD_ENABLE_BLE
  NimBLEDevice::init("GeyserSwitch");
  initGatt();
#endif
}

void BleBackendNimble::loop() {
  if (!active_) return;
  // Placeholder: BLE event processing will be added when NimBLE is wired.
}

void BleBackendNimble::activate(bool on) {
  active_ = on;
  if (on) {
    Logger::info("BLE: activated (skeleton)");
#if BUILD_ENABLE_BLE
    startAdvertising();
#endif
  } else {
    Logger::info("BLE: deactivated (skeleton)");
#if BUILD_ENABLE_BLE
    stopAdvertising();
#endif
  }
}

bool BleBackendNimble::publishTempC(float tempC) {
  if (!active_) return false;
  #if BUILD_ENABLE_BLE
  if (cTempC_) {
    ((NimBLECharacteristic*)cTempC_)->setValue((uint8_t*)&tempC, sizeof(float));
    ((NimBLECharacteristic*)cTempC_)->notify();
  }
  #endif
  return true;
}

bool BleBackendNimble::publishRelayState(bool on) {
  if (!active_) return false;
  #if BUILD_ENABLE_BLE
  if (cState_) {
    // Use 1 byte for boolean
    const uint8_t v = on ? 1 : 0; ((NimBLECharacteristic*)cState_)->setValue(&v, 1); 
    ((NimBLECharacteristic*)cState_)->notify();
  }
  #endif
  return true;
}

bool BleBackendNimble::publishLastUpdate(const String& hhmmss, const String& yyyymmdd) {
  if (!active_) return false;
  #if BUILD_ENABLE_BLE
  if (cTime_) { ((NimBLECharacteristic*)cTime_)->setValue(hhmmss.c_str()); 
    ((NimBLECharacteristic*)cTime_)->notify(); }
  if (cDate_) { ((NimBLECharacteristic*)cDate_)->setValue(yyyymmdd.c_str()); 
    ((NimBLECharacteristic*)cDate_)->notify(); }
  #endif
  return true;
}

void BleBackendNimble::subscribeRelayCommand(RelayCallback onChange, void* ctx) {
  relayCb_ = onChange;
  relayCtx_ = ctx;
}

bool BleBackendNimble::ensureMaxTemp(float defaultCelsius, float &outCelsius) {
  outCelsius = settings_.maxTempC;
  if (outCelsius <= 0.0f) {
    settings_.maxTempC = defaultCelsius;
    outCelsius = defaultCelsius;
  }
  return true;
}

bool BleBackendNimble::ensureHysteresis(float defaultCelsius, float &outCelsius) {
  // For now, reuse default; store separately later if needed
  if (hysteresisC_ <= 0.0f) hysteresisC_ = defaultCelsius;
  outCelsius = hysteresisC_;
  return true;
}

bool BleBackendNimble::ensureTimerFlag(const String &key, bool defaultEnabled, bool &outEnabled) {
  bool *slot = nullptr;
  if (key == "04:00") slot = &settings_.t0400;
  else if (key == "06:00") slot = &settings_.t0600;
  else if (key == "08:00") slot = &settings_.t0800;
  else if (key == "16:00") slot = &settings_.t1600;
  else if (key == "18:00") slot = &settings_.t1800;
  if (!slot) return false;
  if (!*slot) *slot = defaultEnabled;
  outEnabled = *slot;
  return true;
}

bool BleBackendNimble::ensureCustomTime(const String &defaultHhmm, String &outHhmm) {
  if (settings_.customTime.length() != 5) {
    settings_.customTime = defaultHhmm;
  }
  outHhmm = settings_.customTime;
  return true;
}

bool BleBackendNimble::setStringPath(const String &/*path*/, const String &/*value*/) {
  // Usage and misc string paths can be implemented later; return true to avoid failing callers
  return true;
}

bool BleBackendNimble::setIntPath(const String &/*path*/, int /*value*/) {
  return true;
}

bool BleBackendNimble::getIntPath(const String &/*path*/, int &/*outValue*/) {
  return false;
}

void BleBackendNimble::setTimersBitmask(uint8_t mask, const String &defaultCustom) {
  bool t0400 = false, t0600 = false, t0800 = false, t1600 = false, t1800 = false, custom = false;
  BleUuids::unpackTimers(mask, t0400, t0600, t0800, t1600, t1800, custom);
  settings_.t0400 = t0400;
  settings_.t0600 = t0600;
  settings_.t0800 = t0800;
  settings_.t1600 = t1600;
  settings_.t1800 = t1800;
  // Custom mapping: presence of valid CustomTime string is enable flag
  if (custom) {
    if (settings_.customTime.length() != 5) {
      settings_.customTime = defaultCustom;
    }
  } else {
    // Disable custom by clearing the time string
    settings_.customTime.remove(0);
  }
}

uint8_t BleBackendNimble::getTimersBitmask() const {
  bool custom = settings_.customTime.length() == 5;
  return BleUuids::packTimers(settings_.t0400, settings_.t0600, settings_.t0800, settings_.t1600, settings_.t1800, custom);
}

#if BUILD_ENABLE_BLE

class BleBackendNimble::CharWriteCb : public NimBLECharacteristicCallbacks {
 public:
  explicit CharWriteCb(BleBackendNimble *owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic* c) {
    if (!owner_) return;
    std::string uuid = c->getUUID().toString();
    if (uuid == BleUuids::CHAR_COMMAND) {
      auto v = c->getValue();
      if (v.length() > 0) {
        const uint8_t *p = (const uint8_t*)v.data();
        bool on = p[0] != 0;
        if (owner_->relayCb_) owner_->relayCb_(on, owner_->relayCtx_);
      }
    } else if (uuid == BleUuids::CHAR_MAXTEMPC) {
      auto val = c->getValue();
      float f = 0.0f; memcpy(&f, val.data(), std::min((size_t)sizeof(float), (size_t)val.length()));
      owner_->settings_.maxTempC = f;
    } else if (uuid == BleUuids::CHAR_HYSTERESISC) {
      auto val = c->getValue();
      float f = 0.0f; memcpy(&f, val.data(), std::min((size_t)sizeof(float), (size_t)val.length()));
      owner_->hysteresisC_ = f;
    } else if (uuid == BleUuids::CHAR_TIMERS_BITMASK) {
      auto val = c->getValue();
      uint8_t m = 0; if (val.length() > 0) m = (uint8_t)((const uint8_t*)val.data())[0];
      owner_->setTimersBitmask(m);
    } else if (uuid == BleUuids::CHAR_CUSTOMTIME) {
      auto val = c->getValue();
      std::string s((const char*)val.data(), val.length());
      String hhmm = String(s.c_str());
      hhmm.trim();
      if (hhmm.length() == 5) owner_->settings_.customTime = hhmm; else owner_->settings_.customTime.remove(0);
    } else if (uuid == BleUuids::CHAR_TIMESYNC_EPOCH) {
      auto val = c->getValue();
      uint32_t epoch = 0; memcpy(&epoch, val.data(), std::min((size_t)sizeof(uint32_t), (size_t)val.length()));
      struct timeval tv; tv.tv_sec = epoch; tv.tv_usec = 0; settimeofday(&tv, nullptr);
    }
    owner_->updateCharacteristicMirrors();
  }
 private:
  BleBackendNimble *owner_;
};

void BleBackendNimble::initGatt() {
  server_ = NimBLEDevice::createServer();
  service_ = ((NimBLEServer*)server_)->createService(BleUuids::SERVICE_GEYSERSWITCH);
  auto *svc = (NimBLEService*)service_;

  cCmd_ = svc->createCharacteristic(BleUuids::CHAR_COMMAND, NIMBLE_PROPERTY::WRITE);
  cState_ = svc->createCharacteristic(BleUuids::CHAR_STATE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  cTempC_ = svc->createCharacteristic(BleUuids::CHAR_TEMPC, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  cMaxC_ = svc->createCharacteristic(BleUuids::CHAR_MAXTEMPC, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  cHystC_ = svc->createCharacteristic(BleUuids::CHAR_HYSTERESISC, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  cTimers_ = svc->createCharacteristic(BleUuids::CHAR_TIMERS_BITMASK, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  cCustom_ = svc->createCharacteristic(BleUuids::CHAR_CUSTOMTIME, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  cTime_ = svc->createCharacteristic(BleUuids::CHAR_LASTUPDATETIME, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  cDate_ = svc->createCharacteristic(BleUuids::CHAR_LASTUPDATEDATE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  cTimeSync_ = svc->createCharacteristic(BleUuids::CHAR_TIMESYNC_EPOCH, NIMBLE_PROPERTY::WRITE);
  cUsageTotal_ = svc->createCharacteristic(BleUuids::CHAR_USAGE_TOTAL_TODAY, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  auto *cb = new CharWriteCb(this);
  ((NimBLECharacteristic*)cCmd_)->setCallbacks(cb);
  ((NimBLECharacteristic*)cMaxC_)->setCallbacks(cb);
  ((NimBLECharacteristic*)cHystC_)->setCallbacks(cb);
  ((NimBLECharacteristic*)cTimers_)->setCallbacks(cb);
  ((NimBLECharacteristic*)cCustom_)->setCallbacks(cb);
  ((NimBLECharacteristic*)cTimeSync_)->setCallbacks(cb);

  svc->start();
}

void BleBackendNimble::startAdvertising() {
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BleUuids::SERVICE_GEYSERSWITCH);
  adv->start();
  updateCharacteristicMirrors();
}

void BleBackendNimble::stopAdvertising() {
  NimBLEDevice::getAdvertising()->stop();
}

void BleBackendNimble::updateCharacteristicMirrors() {
  if (!service_) return;
  if (cState_) {
    // No direct relay state here; upper layer will publish via publishRelayState soon
  }
  if (cTempC_) {
    // Value set by publishTempC
  }
  if (cMaxC_) {
    float f = settings_.maxTempC; ((NimBLECharacteristic*)cMaxC_)->setValue((uint8_t*)&f, sizeof(float));
  }
  if (cHystC_) {
    float f = hysteresisC_; ((NimBLECharacteristic*)cHystC_)->setValue((uint8_t*)&f, sizeof(float));
  }
  if (cTimers_) {
    uint8_t m = getTimersBitmask(); ((NimBLECharacteristic*)cTimers_)->setValue(&m, 1);
  }
  if (cCustom_) {
    ((NimBLECharacteristic*)cCustom_)->setValue(settings_.customTime.c_str());
  }
}

#endif


