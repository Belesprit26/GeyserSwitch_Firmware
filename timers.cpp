#include "timers.h"
#include "firebase_functions.h"
#include "date_util.h"
#include "temp_control.h"

// Global variables to store the trigger times (hours only) and states
const String triggerHours[] = {"04", "06", "08", "16", "18", "20"};
const String timerPaths[] = {"/Timers/04:00", "/Timers/06:00", "/Timers/08:00", "/Timers/16:00", "/Timers/18:00", "/Timers/20:00"};
bool actionTriggeredTodayGeyser[] = {false, false, false, false, false, false};

// Custom timer path
const String customTimerPath = "/Timers/CUSTOM";
bool customActionTriggered = false;

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
        Serial.println(dbGetBool(gsFree + timerPaths[i]));

        // Check if the timer is active and the current hour matches the trigger hour
        if (dbGetBool(gsFree + timerPaths[i]) && currentHour == triggerHours[i]) {
            if (!actionTriggeredTodayGeyser[i]) {
                setBoolValue(gsFree + geyser_1, true);  // Turn on the geyser
                Serial.println("Geysers turned on at " + triggerHours[i] + ":00");
                actionTriggeredTodayGeyser[i] = true;  // Prevent re-triggering within the same hour
                sendNotification( triggerHours[i] + ":00" + " Timer Alert: Your geyser(s) has turned on.", "Time: " + currentHourMinute);
                Serial.println("STANDARD TIMER TRIGGERED AT: " + triggerHours[i] + ":00" );
            }
        } else if (currentHour != triggerHours[i]) {
            actionTriggeredTodayGeyser[i] = false;  // Reset trigger state for the next time the hour matches
        }
    }

    // Custom timer logic
    String customTimer = dbGetString(gsFree + customTimerPath);  // Read the custom timer value
    customTimer.trim();  // Trim any leading/trailing spaces from Firebase value

    Serial.println("Custom time from Firebase: [" + customTimer + "]");
    Serial.println("Current time: [" + currentHourMinute + "]");

    if (customTimer.length() > 0 && customTimer == currentHourMinute) { // Check if the custom timer matches the current time
        if (!customActionTriggered) {
            setBoolValue(gsFree + geyser_1, true);  // Turn on geyser
            Serial.println("Geysers turned on at custom time: " + customTimer);
            customActionTriggered = true;  // Prevent re-triggering within the same minute
            sendNotification(customTimer +" Custom Timer Alert: Your geyser(s) has turned on.", "Time: " + currentHourMinute);
            Serial.println("CUSTOM TIMER TRIGGERED AT: " + customTimer);
            Serial.println("-----------------------------------");
        }
    } else if (customTimer != currentHourMinute) {
        customActionTriggered = false;  // Reset trigger state for the next time the time matches
    }
}
