// RelayController.h
// Abstract interface for controlling the relay state.

#pragma once

class RelayController {
 public:
  virtual ~RelayController() = default;

  // Sets the relay on or off. Implementations should be idempotent.
  virtual void setOn(bool on) = 0;

  // Returns the last commanded or actual (if readable) state of the relay.
  virtual bool isOn() const = 0;
};


