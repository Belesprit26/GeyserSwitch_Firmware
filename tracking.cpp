#include "tracking.h"
#include "firebase_functions.h" 
#include "ota.h"
#include "date_util.h"    // Use your date_util library for time and date functions
#include <Arduino.h>
#include <Preferences.h>  // Include Preferences library

// Global Preferences instance (needed by ota.cpp)
Preferences preferences;

// --- Helper Functions Using date_util ---
//
// getCurrentTime() returns the current time in "HH:MM" format.
String getCurrentTime() {
    return getFormattedTime("HH:MM");
}

// getCurrentDate() returns the current date in "DD-MM-YYYY" format.
String getCurrentDate() {
    String rawDate = getFormattedDate("DD/MM/YYYY");
    rawDate.replace("/", "-");  // Convert "02/02/2020" to "02-02-2020"
    return rawDate;
}

// --- Structure to Track an Ongoing Cycle ---
struct GeyserTracker {
    bool previousState;           // Last known state: true = ON, false = OFF.
    unsigned long onStartMillis;  // When the cycle started (millis)
    String startTimeStr;          // Formatted start time (HH:MM)
};

// Single geyser tracker
static GeyserTracker geyser1Tracker = { false, 0, "" };

// --- Initialization ---
void initTracking() {
    Serial.println("Firebase tracking system initialized.");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi connected.");
    } else {
        Serial.println("Wi-Fi not connected.");
    }
}

// --- Cycle Tracking Function ---
void trackGeyserUsage(const String& geyserPath, bool currentState) {
    GeyserTracker* tracker = &geyser1Tracker;
    
    // When the geyser turns ON:
    if (currentState && !tracker->previousState) {
        tracker->onStartMillis = millis();
        tracker->startTimeStr = getCurrentTime();
        Serial.println(geyserPath + " turned ON at " + tracker->startTimeStr);
    }
    // When the geyser turns OFF:
    else if (!currentState && tracker->previousState) {
        unsigned long durationSec = (millis() - tracker->onStartMillis) / 1000;
        String endTimeStr = getCurrentTime();
        String currentDate = getCurrentDate();  // e.g., "02-02-2020"
        
        // Create a unique cycle key using the current epoch time.
        time_t now = time(nullptr);
        String cycleKey = "Cycle_" + String(now);
        
        // Build the Firebase path for this cycle:
        String cyclePath = gsFree + "/Records/GeyserDuration/" + currentDate + "/1/" + cycleKey;
        
        // Log the cycle details to Firebase.
        setStringValue(cyclePath + "/StartTime", tracker->startTimeStr);
        setStringValue(cyclePath + "/EndTime", endTimeStr);
        setIntValue(cyclePath + "/Duration", durationSec);
        setIntValue(cyclePath + "/Timestamp", now);
        
        Serial.println("Logged cycle for " + geyserPath + " at " + cyclePath);
        Serial.println("StartTime: " + tracker->startTimeStr +
                       ", EndTime: " + endTimeStr +
                       ", Duration: " + String(durationSec) + " seconds, Timestamp: " + String(now));

        
        
        // --- Update the Daily Total Run Time Accumulator ---
        String totalRunTimePath = gsFree + "/Records/GeyserDuration/" + currentDate + "/1/TotalRunTime";

        int currentTotal = dbGetInt(totalRunTimePath.c_str());
        int newTotal = currentTotal + durationSec;
        
        // Use the update function to add the new duration to the stored total.
        setIntValue(totalRunTimePath, newTotal);
        
        Serial.println("Updated Geyser total run time for " +
                       currentDate + ": " + String(newTotal) + " seconds.");
    }
    
    // Update the tracker’s state for the next call.
    tracker->previousState = currentState;
}

// --- Dummy Definition for Weekly Reset ---
void checkWeeklyReset(const String& realDate) {
    Serial.println("checkWeeklyReset() called with date: " + realDate);
}
