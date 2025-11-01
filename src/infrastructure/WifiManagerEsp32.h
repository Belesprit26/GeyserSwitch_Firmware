// WifiManagerEsp32.h
// Thin, robust wrapper around Arduino WiFi for ESP32.
// - Non-blocking connection with exponential backoff and jitter
// - Simple API for ensuring connectivity and retrieving signal strength

#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "src/infrastructure/Logger.h"

class WifiManagerEsp32 {
 public:
  // Initializes the manager with credentials and kicks off the first connect attempt.
  // This function does not block; it schedules connection attempts.
  void begin(const char* ssid, const char* pass);

  // Progresses the connection state machine. Call regularly from the app loop.
  // Returns true if connected (has WL_CONNECTED and an IP).
  bool ensureConnected();

  // Returns whether the station is connected at the Wi-Fi layer.
  bool isConnected() const;

  // Returns RSSI (dBm) or 0 if not connected.
  int getRssi() const;

  // Returns the local IP address when connected; otherwise 0.0.0.0
  IPAddress localIp() const;

 private:
  enum ConnectState {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_WAIT_BACKOFF,
  };

  String ssid_;
  String pass_;
  ConnectState state_ = STATE_IDLE;
  uint32_t attemptCount_ = 0;
  uint32_t nextAttemptMs_ = 0;

  // Backoff configuration
  static constexpr uint32_t kBaseDelayMs = 1000;        // 1s
  static constexpr uint32_t kMaxDelayMs = 60 * 1000;    // 60s cap
  static constexpr uint32_t kJitterMs   = 250;          // +/- jitter

  void scheduleNextAttempt();
  uint32_t millisNow() const { return millis(); }
};


