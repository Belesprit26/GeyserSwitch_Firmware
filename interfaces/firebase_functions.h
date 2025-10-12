#ifndef FIREBASE_FUNCTIONS_H
#define FIREBASE_FUNCTIONS_H

#include <Arduino.h>
// NOTE: Do not include FirebaseClient headers here. We expose wrapper functions only.

#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W) || defined(ARDUINO_GIGA)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

// Load sensitive config from untracked header
#include "interfaces/secrets.h"

// Do not declare global ssl_client here; managed internally in firebase_functions.cpp

extern unsigned long previousMillis;
extern const long interval;


// Global variables now defined in firebase_functions.cpp
extern String gsFree;
extern String userId;
extern String gsPrime;
extern const String timer1;
extern const String timer2;
extern const String timer3;
extern const String timer4;
extern const String timer5;
extern const String timer7;
extern const String geyser_1;
extern const String sensor_1;
extern const String sensor_1_max;
// Single geyser only
extern const String records;
extern const String updateRecords;

// Function declarations
void setupFirebase();
void controlGeyserBasedOnMaxTemp();
void authHandler();
void authLoop();
bool firebaseIsReady();
bool firebaseInitUserPath();
void asyncInitializeDefaults();
void printError(int code, const String &msg);

// Function declarations with parameters for custom paths and values
bool setIntValue(const String &path, int value);
bool setBoolValue(const String &path, bool value);
bool setFloatValue(const String &path, float value);
bool setDoubleValue(const String &path, double value);
bool setStringValue(const String &path, const String &value);
// Database wrapper getters to avoid exposing Firebase types
bool dbGetBool(const String &path);
String dbGetString(const String &path);
int dbGetInt(const String &path);
float dbGetFloat(const String &path);
double dbGetDouble(const String &path);

// Loop and error helpers
void dbLoop();
int dbLastErrorCode();
String dbLastErrorMessage();

// Function to delete a Firebase path
void deleteFirebasePath(const String &path);
// Function to print Firebase errors
void printErrors(int code, const String &message);

//Default value setting
void setDefaultBoolValue(const String &path, bool defaultValue);
void setDefaultFloatValue(const String &path, float defaultValue);
void setDefaultDoubleValue(const String &path, double defaultValue);
void setDefaultStringValue(const String &path, const String &defaultValue);

// Basic sendNotification function
void sendNotification(String bodyMessage, String dataValue);

// Overloaded sendNotification functions for different data types
void sendNotificationString(String bodyMessage, String dataValue);
void sendNotificationInt(String bodyMessage, int dataValue);
void sendNotificationFloat(String bodyMessage, float dataValue);
void sendNotificationJSON(String bodyMessage, String jsonData);

// Firebase real-time listener setup
void setupFirebaseListeners();

// Config defaults and cached refresh moved to interfaces/config_cache.h
#include "interfaces/config_cache.h"


#endif // FIREBASE_FUNCTIONS_H
