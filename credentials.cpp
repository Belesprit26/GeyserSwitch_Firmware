#include "interfaces/credentials.h"
#include "state/persistence.h"       // State persistence layer

String iSSID = "";
String iPASSWORD = "";
String EMAIL = "";
String PASSWORD = "";


void initEEPROM() {
    // Initialize EEPROM for backward compatibility
    EEPROM.begin(512);  // Initialize EEPROM with 512 bytes
    delay(10);
    
    // Migrate credentials from EEPROM to Preferences if needed
    Persistence::migrateCredentialsFromEEPROM();
}

void readCredentials(String &ssid, String &password, String &email, String &userPassword) {
    // Use Preferences API for credential storage
    Persistence::loadCredentials(ssid, password, email, userPassword);
    
    // Update global variables for backward compatibility
    iSSID = ssid;
    iPASSWORD = password;
    EMAIL = email;
    PASSWORD = userPassword;

    // Debugging logs (print once per boot to avoid repetition)
    static bool printedOnce = false;
    if (!printedOnce) {
        Serial.println("Credentials read from Preferences:");
        Serial.print("SSID: "); Serial.println(ssid);
        Serial.print("Password: "); Serial.println(password);
        Serial.print("Email: "); Serial.println(email);
        Serial.print("User Password: "); Serial.println(userPassword);
        Serial.println(" ");
        printedOnce = true;
    }
}

void saveCredentials(const String &ssid, const String &password, const String &email, const String &userPassword) {
    // Use Preferences API for credential storage
    Persistence::saveCredentials(ssid, password, email, userPassword);
    
    // Update global variables for backward compatibility
    iSSID = ssid;
    iPASSWORD = password;
    EMAIL = email;
    PASSWORD = userPassword;
    
    Serial.println("Credentials saved to Preferences successfully!");
}

void eraseCredentials() {
    // Use Preferences API for credential erasure
    Persistence::eraseCredentials();
    
    // Clear global variables for backward compatibility
    iSSID = "";
    iPASSWORD = "";
    EMAIL = "";
    PASSWORD = "";
    
    Serial.println("Credentials erased from Preferences successfully!");
}

// Updated function with timeout parameter
bool checkAndConnectWiFi(WebServer &server, unsigned long timeout) {
    String ssid, password, email, userPassword;
    readCredentials(ssid, password, email, userPassword);

    if (ssid.length() > 0 && password.length() > 0) {
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.print("Connecting to Wi-Fi: ");
        Serial.println(ssid);

        unsigned long startAttemptTime = millis();

        // Attempt to connect within the specified timeout
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
            Serial.print(".");
            delay(300);
            yield(); // Prevent watchdog timer resets
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWi-Fi connected successfully!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            return true;
        } else {
            Serial.println("\nFailed to connect to Wi-Fi within timeout.");
            return false;
        }
    } else {
        static bool warnedNoCreds = false;
        if (!warnedNoCreds) {
            Serial.println("No valid Wi-Fi credentials found in Preferences.");
            warnedNoCreds = true;
        }
        return false;
    }
}
