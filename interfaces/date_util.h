#ifndef DATE_UTIL_H
#define DATE_UTIL_H

#include <Arduino.h>

#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W) || defined(ARDUINO_GIGA)
#include <WiFi.h>
#include <time.h>  // For time functions
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <time.h>  // For time functions
#endif

// Declare global variables as extern
extern String realTime;
extern String realDate;

// Function declarations
void initializeAndSyncTime(String &date, String &time);
void initializeTime(const char* ntpServer, long gmtOffset_sec, int daylightOffset_sec);
String getFormattedDate(const String& dateFormat = "YYYY-MM-DD");
String getFormattedTime(const String& timeFormat = "HH:MM:SS");

// Declare the checkAndHandleRestart function
void checkAndHandleRestart(struct tm timeinfo);

#endif // DATE_UTIL_H
