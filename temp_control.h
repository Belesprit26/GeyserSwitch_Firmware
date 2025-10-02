// temp_control.h
#ifndef TEMP_CONTROL_H
#define TEMP_CONTROL_H

#include <Arduino.h>

// Function declarations
void setupTempSensor();
void controlGeyserBasedOnMaxTemp();

extern double Temperature1;

#endif // TEMP_CONTROL_H
