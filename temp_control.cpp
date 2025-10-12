#include "interfaces/temp_control.h"
#include "interfaces/firebase_functions.h"
#include "interfaces/date_util.h"

// Temperature sensor variable
static double Temperature1 = 0.0;
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Arduino.h> // For isnan() and isinf() checks

// GPIO pins for the two sensors (updated for ESP32-C6)
#define ONE_WIRE_BUS_1 21   // Pin for sensor 1

// OneWire and DallasTemperature instances for DS18B20 sensors
OneWire oneWire(ONE_WIRE_BUS_1);
DallasTemperature sensors(&oneWire);

// Global variables to store temperatures - now managed by AppState
// Temperature sensor variable now defined as static in this file

void setupTempSensor() {
    // Initialize DallasTemperature sensors
    sensors.begin();
    Serial.println("DS18B20 temperature sensors initialized");
}

// Function to read temperature from sensor 1
double readTemperatureSensor1() {
    // Request temperature conversion from all sensors
    sensors.requestTemperatures();

    // Read temperature from first sensor (index 0)
    float tempC = sensors.getTempCByIndex(0);

    // Check for sensor read error (-127.00 typically indicates no sensor)
    if (tempC == DEVICE_DISCONNECTED_C) {
        Serial.println("DS18B20 sensor not detected - using default temperature: 204.00°C");
        return 204.00;  // Default temperature when sensor is not connected
    }

    // Store and return temperature
    Temperature1 = static_cast<double>(tempC);
    return Temperature1;
}

// Function to read temperature from sensor 2
// Removed second sensor

void controlGeyserBasedOnMaxTemp() {
    // Reading temperatures from both sensors
    double currentTemp1 = readTemperatureSensor1();
    // Single sensor only

    // Update the current temperatures in Firebase for both sensors (only if finite and Firebase is ready)
    if (firebaseIsReady() && !isnan(currentTemp1) && !isinf(currentTemp1)) {
        setFloatValue(gsFree + sensor_1, currentTemp1);
    } else if (!firebaseIsReady()) {
        Serial.println("Firebase not ready - skipping temperature upload");
    } else {
        Serial.println("Warning: Skipping Firebase write for sensor 1 due to invalid temperature (NaN or Inf). Check DS18B20 wiring.");
    }
    // Removed sensor_2 updates

    // Only proceed with Firebase-dependent operations if Firebase is ready
    if (firebaseIsReady()) {
        // Fetch the max temperatures for both sensors
        refreshConfigCache10s();
        double maxTemp1 = getMaxTemp1Cached();
        // Removed sensor_2 max fetch

        // Logging temperatures
        Serial.println("Sensor 1 - Current Temperature: " + String(currentTemp1, 2) + " °C");
        Serial.println("Sensor 1 - Max Temperature: " + String(maxTemp1, 2) + " °C");
        // Removed sensor 2 logs

        // Fetch the current status of geysers
        bool geyserStatus1 = (digitalRead(geyser_1_pin) == HIGH); // Check current geyser 1 status from pin
        // Removed geyser 2 status

        // Controlling geyser based on sensor 1
        // Skip temperature control if using default 204.00°C (sensor not connected)
        if (currentTemp1 != 204.00 && currentTemp1 >= maxTemp1 && geyserStatus1) {
            Serial.printf("🌡️ TEMP CONTROL: Temperature %.2f°C >= Max %.2f°C - Turning geyser OFF\n", currentTemp1, maxTemp1);
            setBoolValue(gsFree + geyser_1, false);  // Turn off geyser 1
            digitalWrite(geyser_1_pin, LOW);  // Turn off geyser pin directly
            sendNotification("Geyser 1 has reached the desired temperature: " +
                             String(currentTemp1, 2) + "°C" + " . The geyser has been turned off", "at: " + realTime);
            Serial.println("Geyser 1 turned off due to sensor 1 exceeding max temperature.");
        } else if (currentTemp1 == 204.00) {
            Serial.println("Skipping temperature control - using default temperature (sensor not connected).");
        }
    } else {
        // Firebase not ready - just log temperature locally
        Serial.println("Sensor 1 - Current Temperature: " + String(currentTemp1, 2) + " °C (Firebase not ready)");
    }

    // Controlling geyser based on sensor 2
    // Removed second geyser control
}
