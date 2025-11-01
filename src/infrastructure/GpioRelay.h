// GpioRelay.h
// Simple GPIO-based relay/LED controller. Supports active-LOW modules or LEDs.

#pragma once

#include <Arduino.h>

class GpioRelay {
 public:
  // Construct a relay controller. Call begin() before use.
  explicit GpioRelay(bool activeLow = true) : activeLow_(activeLow) {}

  // Initialize the GPIO pin and set initial OFF state.
  void begin(uint8_t pin) {
    pin_ = pin;
    pinMode(pin_, OUTPUT);
    setOn(false);
  }

  // Set the relay on/off. Idempotent.
  void setOn(bool on) {
    isOn_ = on;
    uint8_t level = activeLow_ ? (on ? HIGH : LOW) : (on ? LOW : HIGH);
    digitalWrite(pin_, level);
  }

  // Return last commanded state.
  bool isOn() const { return isOn_; }

 private:
  uint8_t pin_ = 255;
  bool activeLow_ = false;
  bool isOn_ = false;
};


