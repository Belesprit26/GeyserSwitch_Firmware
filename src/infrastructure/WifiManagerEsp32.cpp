// WifiManagerEsp32.cpp

#include "WifiManagerEsp32.h"

void WifiManagerEsp32::begin(const char* ssid, const char* pass) {
  ssid_ = ssid ? ssid : "";
  pass_ = pass ? pass : "";

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // we'll manage retries
  WiFi.persistent(false);

  state_ = STATE_CONNECTING;
  attemptCount_ = 0;
  nextAttemptMs_ = 0;

  Logger::info("WiFi: starting connection to SSID '%s'", ssid_.c_str());
  WiFi.begin(ssid_.c_str(), pass_.c_str());
}

bool WifiManagerEsp32::ensureConnected() {
  // Already connected?
  if (WiFi.status() == WL_CONNECTED) {
    if (state_ != STATE_IDLE) {
      Logger::info("WiFi: connected, IP=%s", WiFi.localIP().toString().c_str());
      state_ = STATE_IDLE;  // steady state
    }
    return true;
  }

  // Not connected: handle state machine
  switch (state_) {
    case STATE_CONNECTING: {
      wl_status_t s = WiFi.status();
      if (s == WL_CONNECTED) {
        Logger::info("WiFi: connected, IP=%s", WiFi.localIP().toString().c_str());
        state_ = STATE_IDLE;
      } else if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL || s == WL_DISCONNECTED) {
        Logger::warn("WiFi: connect status=%d, scheduling retry", static_cast<int>(s));
        scheduleNextAttempt();
        state_ = STATE_WAIT_BACKOFF;
      }
      break;
    }
    case STATE_WAIT_BACKOFF: {
      uint32_t now = millisNow();
      if (now >= nextAttemptMs_) {
        Logger::info("WiFi: retrying (attempt %lu) to '%s'", (unsigned long)attemptCount_ + 1, ssid_.c_str());
        WiFi.disconnect(true);
        delay(50);
        WiFi.begin(ssid_.c_str(), pass_.c_str());
        state_ = STATE_CONNECTING;
      }
      break;
    }
    case STATE_IDLE: {
      // Lost connection; go to retry with backoff
      Logger::warn("WiFi: lost connection, scheduling retry");
      scheduleNextAttempt();
      state_ = STATE_WAIT_BACKOFF;
      break;
    }
  }

  return false;
}

bool WifiManagerEsp32::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

int WifiManagerEsp32::getRssi() const {
  if (!isConnected()) return 0;
  return WiFi.RSSI();
}

IPAddress WifiManagerEsp32::localIp() const {
  if (!isConnected()) return IPAddress(0, 0, 0, 0);
  return WiFi.localIP();
}

void WifiManagerEsp32::scheduleNextAttempt() {
  // Exponential backoff with jitter: delay = min(max, base * 2^attempt) +/- jitter
  uint32_t expDelay = kBaseDelayMs;
  if (attemptCount_ > 0) {
    // Cap exponent to avoid overflow
    uint32_t factor = 1u << min<uint32_t>(attemptCount_, 10u);  // up to 1024x
    expDelay = kBaseDelayMs * factor;
  }
  if (expDelay > kMaxDelayMs) expDelay = kMaxDelayMs;

  // Jitter in range [-kJitterMs, +kJitterMs]
  int32_t jitter = (int32_t)(random(0, kJitterMs * 2 + 1)) - (int32_t)kJitterMs;
  int32_t delayWithJitter = (int32_t)expDelay + jitter;
  if (delayWithJitter < 0) delayWithJitter = 0;

  nextAttemptMs_ = millisNow() + (uint32_t)delayWithJitter;
  attemptCount_++;
  Logger::debug("WiFi: backoff %ld ms (attempt %lu)", (long)delayWithJitter, (unsigned long)attemptCount_);
}


