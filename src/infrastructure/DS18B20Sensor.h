// DS18B20Sensor.h
// DS18B20 temperature sensor driver using OneWire + DallasTemperature.
// Responsibilities:
// - Initialize bus and detect sensor presence
// - Read Celsius temperature with basic validation
// - Handle "device not found" and CRC errors gracefully

#pragma once

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class DS18B20Sensor {
 public:
  // Construct the sensor driver (no I/O yet). Call begin() before use.
  DS18B20Sensor();

  // Initialize the OneWire bus on the specified dataPin. Returns true on success.
  // If no devices are found, returns false but the instance remains usable; reads will fail until a device is present.
  bool begin(uint8_t dataPin);

  // Attempt to read the temperature in Celsius into outTempC. Returns true on success.
  // On failure (no device, CRC error, disconnected), returns false.
  bool readCelsius(float &outTempC);

  // Returns whether at least one device was detected during the last begin() call.
  bool hasDevice() const { return hasDevice_; }

 private:
  OneWire *oneWire_ = nullptr;
  DallasTemperature *sensors_ = nullptr;
  bool hasDevice_ = false;
};


