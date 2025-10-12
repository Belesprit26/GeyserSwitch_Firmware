#include "interfaces/timers.h"
#include "interfaces/firebase_functions.h"
#include "interfaces/date_util.h"
#include "interfaces/temp_control.h"
#include "interfaces/tracking.h"

// Timer flags to prevent re-triggering
static bool actionTriggeredTodayGeyser[6] = {false, false, false, false, false, false};
static bool customActionTriggered = false;

// Geyser pin (defined in main sketch)
extern const int geyser_1_pin;

// Global variables to store the trigger times (hours only) and states
const String triggerHours[] = {"04", "06", "08", "16", "18"};
const String timerPaths[] = {"/Timers/04:00", "/Timers/06:00", "/Timers/08:00", "/Timers/16:00", "/Timers/18:00"};

// Timer trigger states - now managed by AppState
// Timer flags now defined as static variables in this file

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

    // Refresh cache ONCE before checking all timers
    refreshConfigCache10s();

    // Regular timers logic
    Serial.printf("🕐 TIMER CHECK: Current time %s - Checking %d timers\n", currentHourMinute.c_str(), 6);
    for (int i = 0; i < 6; i++) {
        bool timerVal = getTimerCached(i);
        Serial.printf("  Timer[%d] %s: %s\n", i, triggerHours[i].c_str(), timerVal ? "ON" : "OFF");
        
        // Only print when timer state changes or triggers
        if (timerVal) {
            Serial.printf("Timer %d (%s): ON\n", i + 1, timerPaths[i].c_str());
        }

        // Check if the timer is active and the current hour matches the trigger hour
        if (timerVal && currentHour == triggerHours[i]) {
            if (!actionTriggeredTodayGeyser[i]) {
                Serial.printf("⏰ TIMER TRIGGER: Timer[%d] at %s:00 - Turning geyser ON\n", i, triggerHours[i].c_str());
                setBoolValue(gsFree + geyser_1, true);  // Turn on the geyser
                digitalWrite(geyser_1_pin, HIGH);
                trackGeyserUsage(geyser_1, true);  // Track usage
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
    String customTimer = getCustomTimerCached();  // Use cached value instead of direct Firebase read
    customTimer.trim();  // Trim any leading/trailing spaces from Firebase value

    Serial.printf("🕐 CUSTOM TIMER: [%s] Current: [%s] Match: %s\n", 
                  customTimer.length() > 0 ? customTimer.c_str() : "EMPTY",
                  currentHourMinute.c_str(),
                  (customTimer.length() > 0 && customTimer == currentHourMinute) ? "YES" : "NO");

    if (customTimer.length() > 0 && customTimer == currentHourMinute) { // Check if the custom timer matches the current time
        if (!customActionTriggered) {
            Serial.printf("⏰ CUSTOM TIMER TRIGGER: %s - Turning geyser ON\n", customTimer.c_str());
            setBoolValue(gsFree + geyser_1, true);  // Turn on geyser
            digitalWrite(geyser_1_pin, HIGH);
            trackGeyserUsage(geyser_1, true);  // Track usage
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
