// Pins.h
// Logical pin mapping for ESP32-C6 board. Adjust to your hardware.

#pragma once

// DS18B20 data pin; requires a 4.7k pull-up resistor to 3.3V.
#ifndef PIN_DS18B20_DATA
#define PIN_DS18B20_DATA 21
#endif

// Relay control pin; typical modules are active-LOW (LOW = ON).
// 23 is correct. TEST: use onboard LED (GPIO 15) for geyser output
#ifndef PIN_RELAY_CTRL
#define PIN_RELAY_CTRL 15
#endif


