#include "interfaces/date_util.h"

// NTP server and time zone settings for Cape Town
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;   // Cape Town is GMT+2
const int   daylightOffset_sec = 0;  // Cape Town does not observe daylight savings

// Define global variables
String realTime;
String realDate;

void initializeAndSyncTime(String &date, String &time) {
    // Initialize time synchronization
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("Time synchronization started.");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time. Retrying...");
        while (!getLocalTime(&timeinfo)) {
            delay(1000);
            Serial.println("Retrying to get time...");
        }
    }

    // Get and set the formatted date and time
    date = getFormattedDate("YYYY-MM-DD");  // Only date
    time = getFormattedTime("HH:MM:SS");    // Only time

    Serial.println("Current Date: " + date);
    Serial.println("Current Time: " + time);

    // Handle restart logic
    checkAndHandleRestart(timeinfo);
}

void checkAndHandleRestart(struct tm timeinfo) {
    // Check if it's the 1st of the month
    if (timeinfo.tm_mday == 1) {
        // Check if it's 6 hours before midday (i.e., 06:00)
        if (timeinfo.tm_hour == 6) {
            String month = getFormattedDate("Month/Day").substring(0, 3);  // Extract month name
            Serial.println("Your module will refresh and restart today at Midday the 1st of " + month + ". Please don't forget to check your settings once recalibrated.");
        }

        // Check if it's midday (i.e., 12:00)
        if (timeinfo.tm_hour == 12) {
            Serial.println("Restarting device...");
            delay(5000); // Optional: delay to allow Serial messages to be sent before restart
            ESP.restart(); // Restart the ESP32 or ESP8266
        }
    }
}

void initializeTime(const char* ntpServer, long gmtOffset_sec, int daylightOffset_sec) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("Time synchronization started");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println("Time synchronized");
}

String getFormattedDate(const String& dateFormat) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return "";
    }

    char buffer[30];
    if (dateFormat == "YYYY-MM-DD") {
        strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
    } else if (dateFormat == "DD/MM/YYYY") {
        strftime(buffer, sizeof(buffer), "%d/%m/%Y", &timeinfo);
    } else if (dateFormat == "MM-DD-YYYY") {
        strftime(buffer, sizeof(buffer), "%m-%d-%Y", &timeinfo);
    } else if (dateFormat == "Month/Day") {
        // Custom format: "April/Mon 27th"
        strftime(buffer, sizeof(buffer), "%B/%a %d", &timeinfo);
        int day = timeinfo.tm_mday;
        String suffix;
        if (day == 1 || day == 21 || day == 31) {
            suffix = "st";
        } else if (day == 2 || day == 22) {
            suffix = "nd";
        } else if (day == 3 || day == 23) {
            suffix = "rd";
        } else {
            suffix = "th";
        }
        return String(buffer) + suffix;
    } else {
        // Default format
        strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
    }

    return String(buffer);
}

String getFormattedTime(const String& timeFormat) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return "";
    }

    char buffer[20];
    if (timeFormat == "HH:MM:SS") {
        strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    } else if (timeFormat == "HH:MM") {
        strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
    } else if (timeFormat == "hh:MM:SS AM/PM") {
        strftime(buffer, sizeof(buffer), "%I:%M:%S %p", &timeinfo);
    } else {
        // Default format
        strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    }

    return String(buffer);
}
