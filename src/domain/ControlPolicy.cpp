// ControlPolicy.cpp

#include "ControlPolicy.h"

namespace ControlPolicy {

static inline bool belowReenableThreshold(float tempC, float maxTempC, float hysteresisC) {
  return tempC < (maxTempC - hysteresisC);
}

ControlDecision evaluate(const ControlInputs &in) {
  ControlDecision d{};

  // 1) Safety cutoff always first
  if (in.tempC >= in.maxTempC) {
    d.relayOn = false;
    return d;
  }

  // 2) Remote command if present (subject to safety already applied)
  if (in.hasCommand) {
    if (in.commandOn) {
      // Turn ON only if sufficiently below cutoff (hysteresis)
      d.relayOn = belowReenableThreshold(in.tempC, in.maxTempC, in.hysteresisC);
    } else {
      d.relayOn = false;
    }
    return d;
  }

  // 3) Schedule if no command
  if (in.scheduleActive) {
    d.relayOn = belowReenableThreshold(in.tempC, in.maxTempC, in.hysteresisC);
  } else {
    d.relayOn = false;
  }
  return d;
}

}  // namespace ControlPolicy


