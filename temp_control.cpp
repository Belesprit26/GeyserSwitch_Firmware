#include "temp_control.h"
#include "firebase_functions.h"
#include "date_util.h"
#include <WiFi.h>
#include <OneWireNg_CurrentPlatform.h>
#include <drivers/DSTherm.h>
#include <utils/Placeholder.h>
#include <Arduino.h> // For isnan() and isinf() checks

// GPIO pins for the two sensors (updated for ESP32-C6)
#define ONE_WIRE_BUS_1 21   // Pin for sensor 1

// OneWireNg instance for ESP32-C6 current platform (enable internal pull-up)
static OneWireNg_CurrentPlatform ow(ONE_WIRE_BUS_1, true);
static DSTherm drv(ow);

// Global variables to store temperatures
double Temperature1 = 0.0;

void setupTempSensor() {
    // Nothing required for OneWireNg; bus is initialized via constructor
}

// Function to read temperature from sensor 1
double readTemperatureSensor1() {
    // Start temperature conversion for all sensors and wait max time
    OneWireNg::ErrorCode ec = drv.convertTempAll(DSTherm::MAX_CONV_TIME, false);
    if (ec != OneWireNg::EC_SUCCESS) {
        return NAN;
    }

    // Read scratchpad for a single sensor environment
    Placeholder<DSTherm::Scratchpad, true> scrpd;
    ec = drv.readScratchpadSingle(scrpd, true);
    if (ec != OneWireNg::EC_SUCCESS) {
        return NAN;
    }

    // Convert to Celsius
    Temperature1 = static_cast<double>(scrpd->getTemp()) / 1000.0;
    return Temperature1;
}

// Function to read temperature from sensor 2
// Removed second sensor

void controlGeyserBasedOnMaxTemp() {
    
    // Reading temperatures from both sensors
    double currentTemp1 = readTemperatureSensor1();
    // Single sensor only

    // Update the current temperatures in Firebase for both sensors (only if finite to avoid NaN errors)
    if (!isnan(currentTemp1) && !isinf(currentTemp1)) {
        setFloatValue(gsFree + sensor_1, currentTemp1);
    } else {
        Serial.println("Warning: Skipping Firebase write for sensor 1 due to invalid temperature (NaN or Inf). Check DS18B20 wiring.");
    }
    // Removed sensor_2 updates

    // Fetch the max temperatures for both sensors
    double maxTemp1 = dbGetDouble(gsFree + sensor_1_max);
    // Removed sensor_2 max fetch

    // Logging temperatures
    Serial.println("Sensor 1 - Current Temperature: " + String(currentTemp1, 2) + " °C");
    Serial.println("Sensor 1 - Max Temperature: " + String(maxTemp1, 2) + " °C");
    // Removed sensor 2 logs

    // Fetch the current status of geysers
    bool geyserStatus1 = dbGetBool(gsFree + geyser_1); // Check current geyser 1 status
    // Removed geyser 2 status

    // Controlling geyser based on sensor 1
    if (currentTemp1 >= maxTemp1 && geyserStatus1) {
        setBoolValue(gsFree + geyser_1, false);  // Turn off geyser 1
        sendNotification("Geyser 1 has reached the desired temperature: " +
                         String(currentTemp1, 2) + "°C" + " . The geyser has been turned off", "at: " + realTime);
        Serial.println("Geyser 1 turned off due to sensor 1 exceeding max temperature.");
    }
    
    // Controlling geyser based on sensor 2
    // Removed second geyser control
}
