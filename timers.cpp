#include "interfaces/timers.h"
#include "interfaces/firebase_functions.h"
#include "interfaces/date_util.h"
#include "interfaces/temp_control.h"
#include "interfaces/tracking.h"
#include "state/app_state.h"         // Centralized state management
#include "state/persistence.h"       // State persistence layer

// Global variables to store the trigger times (hours only) and states
const String triggerHours[] = {"04", "06", "08", "16", "18"};
const String timerPaths[] = {"/Timers/04:00", "/Timers/06:00", "/Timers/08:00", "/Timers/16:00", "/Timers/18:00"};

// Timer trigger states - now managed by AppState
// bool actionTriggeredTodayGeyser[] = {false, false, false, false, false, false}; // → AppState::getTimerFlag(i)
// bool customActionTriggered = false; // → AppState::getCustomTimerFlag()

// Custom timer path
const String customTimerPath = "/Timers/CUSTOM";

void controlGeyserWithTimers() {
    String currentTime = getFormattedTime("HH:mm"); // Get the current time in HH:mm:ss format
     
    String currentHour = currentTime.substring(0, 2);  // Extract the current hour (first 2 characters)
    //Serial.println("Hour: "+ currentHour);
    String currentMinute = currentTime.substring(3, 5);  // Extract the current minute (characters 4 and 5)
    //Serial.println("Minute: "+ currentMinute);
    String currentHourMinute = currentTime.substring(0, 5); // Get the current time in HH:mm format (first 5 characters)
    //Serial.println("Full Time: "+ currentHourMinute);
    //Serial.println("-----------------------------------");

    // Regular timers logic
    for (int i = 0; i < 6; i++) {
        Serial.print("Timer ");
        Serial.print(i + 1);
        Serial.print(" (Path: ");
        Serial.print(timerPaths[i]);
        Serial.print("): ");
        refreshConfigCache10s();
        bool timerVal = getTimerCached(i);
        Serial.println(timerVal);

        // Check if the timer is active and the current hour matches the trigger hour
        if (timerVal && currentHour == triggerHours[i]) {
            if (!AppState::getTimerFlag(i)) {
                setBoolValue(AppState::getGsFree() + geyser_1, true);  // Turn on the geyser
                digitalWrite(15, HIGH);  // TODO: Use geyser_1_pin variable
                AppState::setGeyserOn(true);  // Update centralized state
                trackGeyserUsage(geyser_1, true);  // Track usage
                Serial.println("Geysers turned on at " + triggerHours[i] + ":00");
                AppState::setTimerFlag(i, true);  // Prevent re-triggering within the same hour
                sendNotification( triggerHours[i] + ":00" + " Timer Alert: Your geyser(s) has turned on.", "Time: " + currentHourMinute);
                Serial.println("STANDARD TIMER TRIGGERED AT: " + triggerHours[i] + ":00" );
            }
        } else if (currentHour != triggerHours[i]) {
            AppState::setTimerFlag(i, false);  // Reset trigger state for the next time the hour matches
        }
    }

    // Custom timer logic
    String customTimer = dbGetString(AppState::getGsFree() + customTimerPath);  // Read the custom timer value
    customTimer.trim();  // Trim any leading/trailing spaces from Firebase value

    Serial.println("Custom time from Firebase: [" + customTimer + "]");
    Serial.println("Current time: [" + currentHourMinute + "]");

    if (customTimer.length() > 0 && customTimer == currentHourMinute) { // Check if the custom timer matches the current time
        if (!AppState::getCustomTimerFlag()) {
            setBoolValue(AppState::getGsFree() + geyser_1, true);  // Turn on geyser
            digitalWrite(15, HIGH);  // TODO: Use geyser_1_pin variable
            AppState::setGeyserOn(true);  // Update centralized state
            trackGeyserUsage(geyser_1, true);  // Track usage
            Serial.println("Geysers turned on at custom time: " + customTimer);
            AppState::setCustomTimerFlag(true);  // Prevent re-triggering within the same minute
            sendNotification(customTimer +" Custom Timer Alert: Your geyser(s) has turned on.", "Time: " + currentHourMinute);
            Serial.println("CUSTOM TIMER TRIGGERED AT: " + customTimer);
            Serial.println("-----------------------------------");
        }
    } else if (customTimer != currentHourMinute) {
        AppState::setCustomTimerFlag(false);  // Reset trigger state for the next time the time matches
    }
}
