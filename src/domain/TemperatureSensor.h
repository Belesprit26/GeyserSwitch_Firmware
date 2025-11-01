// TemperatureSensor.h
// Abstract interface for reading temperature in Celsius.

#pragma once

class TemperatureSensor {
 public:
  virtual ~TemperatureSensor() = default;

  // Reads the current temperature in Celsius into outTempC.
  // Returns true on success, false otherwise.
  virtual bool readCelsius(float &outTempC) = 0;
};


