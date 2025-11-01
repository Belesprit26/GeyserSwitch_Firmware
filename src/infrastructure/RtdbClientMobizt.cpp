// RtdbClientMobizt.cpp

#include "RtdbClientMobizt.h"

#if USE_MOBIZT_FIREBASE
// Enable features used by the library (matches examples)
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <WiFiClientSecure.h>
#include <Arduino.h>  // for millis()
typedef WiFiClientSecure SSL_CLIENT;
#include <FirebaseClient.h>
using namespace std;

// Internal implementation kept out of header to avoid leaking library types
struct FirebaseImpl {
  FirebaseApp app;
  // v2 API types per examples
  SSL_CLIENT ssl_client;
  using AsyncClient = AsyncClientClass;
  AsyncClient aClient{ssl_client};
  UserAuth user_auth{SECRETS_FIREBASE_API_KEY, SECRETS_FIREBASE_AUTH_EMAIL, SECRETS_FIREBASE_AUTH_PASS, 3000};
  RealtimeDatabase Database;
  bool configured = false;
  String relayPath;
  bool lastRelayKnown = false;
  bool haveRelayValue = false;
  uint32_t lastPollMs = 0;
  uint32_t lastPollOkMs = 0;
};
#endif

void RtdbClientMobizt::begin(const RtdbPaths* paths) {
  paths_ = paths;

#if USE_MOBIZT_FIREBASE
  Logger::info("RTDB: initializing FirebaseClient");
  if (!impl_) impl_ = new FirebaseImpl();
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);

  // Set SSL client config (use insecure defaults and buffers as needed)
  // Note: ExampleFunctions.h provides helpers; we proceed without it for now.
  impl->ssl_client.setInsecure();
  

  // Initialize App with user auth
  Logger::info("RTDB: initializing app auth");
  initializeApp(impl->aClient, impl->app, getAuth(impl->user_auth), (unsigned long)(120 * 1000), NULL);

  // Bind Realtime Database and set URL
  impl->app.getApp<RealtimeDatabase>(impl->Database);
  impl->Database.url(SECRETS_FIREBASE_DATABASE_URL);

  // Listen to command path only; device will mirror physical state separately
  impl->relayPath = paths_->geyserCommand();
  impl->configured = true;
  
  // Settings will be pulled periodically; no settings streams
#else
  Logger::warn("RTDB: FirebaseClient disabled (set USE_MOBIZT_FIREBASE=1)");
#endif
}

void RtdbClientMobizt::loop() {
#if USE_MOBIZT_FIREBASE
  if (!active_) return;
#endif
#if USE_MOBIZT_FIREBASE
  // Maintain authentication and async tasks as per examples
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (impl) impl->app.loop();
  const uint32_t nowMs = millis();
  // Poll command path every 2s
  if (impl && impl->configured) {
    const uint32_t interval = 2000u;
    if (nowMs - impl->lastPollMs >= interval) {
      impl->lastPollMs = nowMs;
      bool cmd = impl->Database.get<bool>(impl->aClient, impl->relayPath.c_str());
      if (impl->aClient.lastError().code() == 0) {
        impl->lastPollOkMs = nowMs;
        if (!impl->haveRelayValue || cmd != impl->lastRelayKnown) {
          impl->haveRelayValue = true;
          impl->lastRelayKnown = cmd;
          if (relayCallback_) relayCallback_(cmd, relayCtx_);
        }
      }
    }
  }
  

  
#endif
}

bool RtdbClientMobizt::publishTempC(float tempC) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  bool ok = impl->Database.set<float>(impl->aClient, paths_->sensorTemp().c_str(), tempC);
  if (!ok) Logger::warn("RTDB: set temp failed");
  return ok;
#else
  (void)tempC; return false;
#endif
}

bool RtdbClientMobizt::publishRelayState(bool on) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  bool ok = impl->Database.set<bool>(impl->aClient, paths_->geyserState().c_str(), on);
  if (!ok) Logger::warn("RTDB: set relay failed");
  return ok;
#else
  (void)on; return false;
#endif
}

bool RtdbClientMobizt::publishLastUpdate(const String& hhmmss, const String& yyyymmdd) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  bool ok1 = impl->Database.set<String>(impl->aClient, paths_->lastUpdateTime().c_str(), hhmmss);
  bool ok2 = impl->Database.set<String>(impl->aClient, paths_->lastUpdateDate().c_str(), yyyymmdd);
  return ok1 && ok2;
#else
  (void)hhmmss; (void)yyyymmdd; return false;
#endif
}

void RtdbClientMobizt::subscribeRelayCommand(RelayCallback onChange, void* ctx) {
  relayCallback_ = onChange;
  relayCtx_ = ctx;
}

// former subscription-based settings functions removed (pull-only strategy)

bool RtdbClientMobizt::getMaxTemp(float &outCelsius) {
#if USE_MOBIZT_FIREBASE
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  outCelsius = impl->Database.get<float>(impl->aClient, paths_->maxTemp().c_str());
  return impl->aClient.lastError().code() == 0;
#else
  (void)outCelsius; return false;
#endif
}

bool RtdbClientMobizt::getTimerFlag(const String &key, bool &outEnabled) {
#if USE_MOBIZT_FIREBASE
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  String path = paths_->timerKey(key.c_str());
  outEnabled = impl->Database.get<bool>(impl->aClient, path.c_str());
  return impl->aClient.lastError().code() == 0;
#else
  (void)key; (void)outEnabled; return false;
#endif
}

bool RtdbClientMobizt::getCustomTime(String &outHhmm) {
#if USE_MOBIZT_FIREBASE
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  // CUSTOM under Timers
  String path = paths_->timersRoot() + "/CUSTOM";
  outHhmm = impl->Database.get<String>(impl->aClient, path.c_str());
  return impl->aClient.lastError().code() == 0;
#else
  (void)outHhmm; return false;
#endif
}

bool RtdbClientMobizt::getHysteresis(float &outCelsius) {
#if USE_MOBIZT_FIREBASE
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  outCelsius = impl->Database.get<float>(impl->aClient, paths_->hysteresisC().c_str());
  return impl->aClient.lastError().code() == 0;
#else
  (void)outCelsius; return false;
#endif
}

bool RtdbClientMobizt::setStringPath(const String &path, const String &value) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  return impl->Database.set<String>(impl->aClient, path.c_str(), value);
#else
  (void)path; (void)value; return false;
#endif
}

bool RtdbClientMobizt::setIntPath(const String &path, int value) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  return impl->Database.set<int>(impl->aClient, path.c_str(), value);
#else
  (void)path; (void)value; return false;
#endif
}

bool RtdbClientMobizt::getIntPath(const String &path, int &outValue) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  outValue = impl->Database.get<int>(impl->aClient, path.c_str());
  return impl->aClient.lastError().code() == 0;
#else
  (void)path; (void)outValue; return false;
#endif
}

bool RtdbClientMobizt::ensureMaxTemp(float defaultCelsius, float &outCelsius) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  if (getMaxTemp(outCelsius)) return true;  // exists
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  bool ok = impl->Database.set<float>(impl->aClient, paths_->maxTemp().c_str(), defaultCelsius);
  if (ok) outCelsius = defaultCelsius;
  if (ok) {
    Logger::info("Settings: created default max_temp=%.2f C", defaultCelsius);
  } else {
    Logger::warn("Settings: failed to create default max_temp (code=%d)", impl->aClient.lastError().code());
  }
  return ok;
#else
  (void)defaultCelsius; (void)outCelsius; return false;
#endif
}

bool RtdbClientMobizt::ensureTimerFlag(const String &key, bool defaultEnabled, bool &outEnabled) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  if (getTimerFlag(key, outEnabled)) return true;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  String path = paths_->timerKey(key.c_str());
  bool ok = impl->Database.set<bool>(impl->aClient, path.c_str(), defaultEnabled);
  if (ok) outEnabled = defaultEnabled;
  if (ok) {
    Logger::info("Settings: created default Timer %s=%s", key.c_str(), defaultEnabled ? "true" : "false");
  } else {
    Logger::warn("Settings: failed to create Timer %s (code=%d)", key.c_str(), impl->aClient.lastError().code());
  }
  return ok;
#else
  (void)key; (void)defaultEnabled; (void)outEnabled; return false;
#endif
}

bool RtdbClientMobizt::ensureCustomTime(const String &defaultHhmm, String &outHhmm) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  if (getCustomTime(outHhmm)) return true;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  String path = paths_->timersRoot() + "/CUSTOM";
  bool ok = impl->Database.set<String>(impl->aClient, path.c_str(), defaultHhmm);
  if (ok) outHhmm = defaultHhmm;
  if (ok) {
    Logger::info("Settings: created default CUSTOM=%s", defaultHhmm.c_str());
  } else {
    Logger::warn("Settings: failed to create CUSTOM (code=%d)", impl->aClient.lastError().code());
  }
  return ok;
#else
  (void)defaultHhmm; (void)outHhmm; return false;
#endif
}

bool RtdbClientMobizt::ensureHysteresis(float defaultCelsius, float &outCelsius) {
#if USE_MOBIZT_FIREBASE
  if (!active_) return false;
  if (getHysteresis(outCelsius)) return true;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  bool ok = impl->Database.set<float>(impl->aClient, paths_->hysteresisC().c_str(), defaultCelsius);
  if (ok) outCelsius = defaultCelsius;
  return ok;
#else
  (void)defaultCelsius; (void)outCelsius; return false;
#endif
}
bool RtdbClientMobizt::isHealthy() const {
#if USE_MOBIZT_FIREBASE
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (!impl || !impl->configured) return false;
  uint32_t nowMs = millis();
  return (nowMs - impl->lastPollOkMs) <= 5000u;
#else
  return false;
#endif
}

void RtdbClientMobizt::activate(bool on) {
#if USE_MOBIZT_FIREBASE
  active_ = on;
  auto *impl = reinterpret_cast<FirebaseImpl*>(impl_);
  if (impl) {
    (void)on;
  }
#else
  (void)on;
#endif
}



