#include "interfaces/ota.h"
#include "interfaces/firebase_functions.h"
#include "state/app_state.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>

extern Preferences preferences;  // Create a Preferences object for storing version information

// Function to initialize Preferences and retrieve the stored version
String getCurrentVersion() {
    preferences.begin("ota", false);  // Open the Preferences namespace "ota"
    String currentVersion = preferences.getString("version", "1.0.12a");  // Default to "1.0.0" if no version is set
    Serial.println("Retrieved current firmware version: " + currentVersion);  // Debug
    preferences.end();  // Close Preferences to free up resources
    return currentVersion;
}

// Function to store the new version in Preferences after a successful OTA update
void storeNewVersion(const String& newVersion) {
    preferences.begin("ota", false);  // Open the Preferences namespace "ota"
    preferences.putString("version", newVersion);  // Store the new firmware version
    Serial.println("Stored new firmware version: " + newVersion);  // Debug
    preferences.end();  // Close Preferences
}

// Function to check for OTA updates in Firebase Realtime Database
void checkForOTAUpdate() {
    // Retrieve the current version from Preferences
    String currentVersion = getCurrentVersion();
    Serial.println("Current firmware version: " + currentVersion);

    // Get the version from Firebase (prioritize global paths, fallback to user-specific if needed)
    String remoteVersion = dbGetString("/firmware/version");  // Global path as per user setup
    if (dbLastErrorCode() != 0 || remoteVersion.length() == 0) {
        // Fallback to user-specific path if global fails
        Serial.println("Global firmware/version path not found, trying user-specific...");
        remoteVersion = dbGetString(AppState::getGsFree() + "/firmware/version");
    }
    if (dbLastErrorCode() == 0 && remoteVersion.length() > 0) {
        Serial.println("Available remote firmware version: " + remoteVersion);  // Debug

        if (currentVersion != remoteVersion) {
            Serial.println("New firmware version available: " + remoteVersion);
            // sendNotification("Scheduled Update: Your GeyserSwitch unit is running an update", "UpdateVersion: " + remoteVersion);

            String firmwareUrl = dbGetString("/firmware/url");  // Global path as per user setup
            if (dbLastErrorCode() != 0 || firmwareUrl.length() == 0) {
                // Fallback to user-specific path if global fails
                Serial.println("Global firmware/url path not found, trying user-specific...");
                firmwareUrl = dbGetString(AppState::getGsFree() + "/firmware/url");
            }
            if (dbLastErrorCode() == 0 && firmwareUrl.length() > 0) {
                Serial.println("Firmware URL: " + firmwareUrl);  // Debug
                performOTAUpdate(firmwareUrl, remoteVersion);  // Perform OTA with the firmware URL and pass the new version
            } else {
                Serial.println("Failed to get firmware URL.");
            }
        } else {
            Serial.println("Firmware is up to date.");
        }
    } else {
        Serial.println("Failed to check for firmware updates. Ensure '/firmware/version' exists in Firebase RTDB and has read permissions.");
    }
}

// Function to perform the actual OTA update
void performOTAUpdate(const String& firmwareUrl, const String& newVersion) {
    Serial.println("Starting OTA update...");

    HTTPClient http;
    http.begin(firmwareUrl);  // Open the URL for the firmware file

    int httpCode = http.GET();  // Send the GET request

    if (httpCode == HTTP_CODE_OK) {  // Check for successful request
        int contentLength = http.getSize();  // Get the firmware size
        WiFiClient* stream = http.getStreamPtr();  // Get the data stream

        if (contentLength > 0) {
            Serial.printf("Firmware size: %d bytes\n", contentLength);

            // Begin OTA update
            if (Update.begin(contentLength)) {
                size_t written = Update.writeStream(*stream);

                if (written == contentLength) {
                    Serial.println("Written: " + String(written) + " successfully");

                    if (Update.end()) {
                        if (Update.isFinished()) {
                            Serial.println("Update successfully completed. Rebooting...");

                            // Store the new version in Preferences after a successful update
                            storeNewVersion(newVersion);

                            ESP.restart();  // Reboot the ESP32
                        } else {
                            Serial.println("Update not finished. Something went wrong.");
                        }
                    } else {
                        Serial.println("Error Occurred: " + String(Update.getError()));
                    }
                } else {
                    Serial.println("Written only: " + String(written) + "/" + String(contentLength));
                }
            } else {
                Serial.println("Not enough space to begin OTA update");
            }
        } else {
            Serial.println("Content-Length not defined.");
        }
    } else {
        Serial.println("Failed to download the firmware. HTTP Error: " + String(httpCode));
        ESP.restart(); 
    }

    http.end();  // Close the HTTP connection
}
