#ifndef LEAK_DETECTION_H
#define LEAK_DETECTION_H

#include <Arduino.h>

// GPIO pins for leak sensor (ESP32-C6 compatible)
#define POWER_PIN 18
#define AO_PIN 4

// Function declaration
void leakDetection();

#endif // LEAK_DETECTION_H
