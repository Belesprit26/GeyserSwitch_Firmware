#ifndef OTA_H
#define OTA_H

#include <Arduino.h>  // Include Arduino core library
#include <Preferences.h>  // Include Preferences library for non-volatile storage

// Function to initialize Preferences and retrieve the current stored version
String getCurrentVersion();

// Function to store the new version in Preferences after a successful OTA update
void storeNewVersion(const String& newVersion);

// Function to check for OTA updates in Firebase Realtime Database
void checkForOTAUpdate();

// Function to perform the actual OTA update
void performOTAUpdate(const String& firmwareUrl, const String& newVersion);

#endif // OTA_H
