// DS18B20Sensor.cpp

#include "DS18B20Sensor.h"

#include "src/infrastructure/Logger.h"

DS18B20Sensor::DS18B20Sensor() {}

bool DS18B20Sensor::begin(uint8_t dataPin) {
  if (oneWire_) { delete oneWire_; oneWire_ = nullptr; }
  if (sensors_) { delete sensors_; sensors_ = nullptr; }

  oneWire_ = new OneWire(dataPin);
  sensors_ = new DallasTemperature(oneWire_);
  sensors_->begin();

  int deviceCount = sensors_->getDeviceCount();
  hasDevice_ = deviceCount > 0;
  if (!hasDevice_) {
    Logger::warn("DS18B20: no devices found on pin %u", (unsigned)dataPin);
  } else {
    Logger::info("DS18B20: %d device(s) found on pin %u", deviceCount, (unsigned)dataPin);
  }
  return hasDevice_;
}

bool DS18B20Sensor::readCelsius(float &outTempC) {
  if (!sensors_) return false;
  sensors_->requestTemperatures();
  float t = sensors_->getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C) {
    // Device missing or CRC error; report failure gracefully
    return false;
  }
  // Basic sanity: DS18B20 typical range -55..125 C
  if (t < -55.0f || t > 125.0f) {
    return false;
  }
  outTempC = t;
  return true;
}


