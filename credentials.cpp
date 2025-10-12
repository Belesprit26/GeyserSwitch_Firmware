#include "interfaces/credentials.h"
#include <EEPROM.h>

String iSSID = "";
String iPASSWORD = "";
String EMAIL = "";
String PASSWORD = "";


// EEPROM helper functions
String readStringFromEEPROM(int address, int maxLength) {
    String result = "";
    for (int i = 0; i < maxLength; i++) {
        char c = EEPROM.read(address + i);
        if (c == 0) break;
        result += c;
    }
    return result;
}

void writeStringToEEPROM(int address, const String &data, int maxLength) {
    int length = min(data.length(), maxLength - 1);
    for (int i = 0; i < length; i++) {
        EEPROM.write(address + i, data[i]);
    }
    EEPROM.write(address + length, 0); // Null terminator
}

void initEEPROM() {
    // Initialize EEPROM for backward compatibility
    EEPROM.begin(512);  // Initialize EEPROM with 512 bytes
    delay(10);
}

void readCredentials(String &ssid, String &password, String &email, String &userPassword) {
    ssid = readStringFromEEPROM(SSID_ADDR, SSID_SIZE);
    password = readStringFromEEPROM(PASSWORD_ADDR, PASSWORD_SIZE);
    email = readStringFromEEPROM(EMAIL_ADDR, EMAIL_SIZE);
    userPassword = readStringFromEEPROM(USER_PASSWORD_ADDR, USER_PASSWORD_SIZE);
    
    // Update global variables for backward compatibility
    iSSID = ssid;
    iPASSWORD = password;
    EMAIL = email;
    PASSWORD = userPassword;

    // Debugging logs (print once per boot to avoid repetition)
    static bool printedOnce = false;
    if (!printedOnce) {
        Serial.println("Credentials read from EEPROM:");
        Serial.print("SSID: "); Serial.println(ssid);
        Serial.print("Password: "); Serial.println(password);
        Serial.print("Email: "); Serial.println(email);
        Serial.print("User Password: "); Serial.println(userPassword);
        Serial.println(" ");
        printedOnce = true;
    }
}

void saveCredentials(const String &ssid, const String &password, const String &email, const String &userPassword) {
    // Save to EEPROM
    writeStringToEEPROM(SSID_ADDR, ssid, SSID_SIZE);
    writeStringToEEPROM(PASSWORD_ADDR, password, PASSWORD_SIZE);
    writeStringToEEPROM(EMAIL_ADDR, email, EMAIL_SIZE);
    writeStringToEEPROM(USER_PASSWORD_ADDR, userPassword, USER_PASSWORD_SIZE);
    EEPROM.commit();
    
    // Update global variables for backward compatibility
    iSSID = ssid;
    iPASSWORD = password;
    EMAIL = email;
    PASSWORD = userPassword;
    
    Serial.println("Credentials saved to EEPROM successfully!");
}

void eraseCredentials() {
    // Clear EEPROM credentials
    for (int i = SSID_ADDR; i < USER_PASSWORD_ADDR + USER_PASSWORD_SIZE; i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
    
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
