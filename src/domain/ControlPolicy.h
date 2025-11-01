// ControlPolicy.h
// Encapsulates relay decision logic using command, schedule and safety with hysteresis.

#pragma once

struct ControlInputs {
  // Optional remote command; when not provided, policy uses schedule+safety only.
  bool hasCommand = false;
  bool commandOn = false;

  // Whether the current time is within an active schedule window
  bool scheduleActive = false;

  // Temperature and thresholds
  float tempC = 0.0f;
  float maxTempC = 60.0f;
  float hysteresisC = 2.0f;  // re-enable only when temp < (maxTempC - hysteresisC)

  // Current relay state (for hysteresis decisions)
  bool relayCurrentlyOn = false;
};

struct ControlDecision {
  bool relayOn = false;
};

namespace ControlPolicy {

// Evaluate the desired relay state given inputs.
// Decision order:
// 1) Safety: if temp >= maxTempC => OFF
// 2) Command: if present => ON/OFF (subject to safety above)
// 3) Schedule: if active and temp < (maxTempC - hysteresis) => ON, else OFF
// 4) Hysteresis prevents ON until temp sufficiently below cutoff
ControlDecision evaluate(const ControlInputs &in);

}


