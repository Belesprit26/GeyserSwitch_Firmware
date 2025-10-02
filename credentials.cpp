#include "credentials.h"

String iSSID = "";
String iPASSWORD = "";
String EMAIL = "";
String PASSWORD = "";


void initEEPROM() {
    EEPROM.begin(512);  // Initialize EEPROM with 512 bytes
    delay(10);
}

void readCredentials(String &ssid, String &password, String &email, String &userPassword) {
    ssid = "";
    password = "";
    email = "";
    userPassword = "";

    // Reading SSID from EEPROM (with validation of printable characters)
    for (int i = 0; i < 32; ++i) {
        char readChar = EEPROM.read(i);
        if (isPrintable(readChar) && readChar != '\0') {
            ssid += readChar;
            iSSID = ssid;
        }
    }

    // Reading password from EEPROM
    for (int i = 32; i < 64; ++i) {
        char readChar = EEPROM.read(i);
        if (isPrintable(readChar) && readChar != '\0') {
            password += readChar;
            iPASSWORD = password;
        }
    }

    // Reading email from EEPROM
    for (int i = 64; i < 192; ++i) {
        char readChar = EEPROM.read(i);
        if (isPrintable(readChar) && readChar != '\0') {
            email += readChar;
            EMAIL = email;
        }
    }

    // Reading user password from EEPROM
    for (int i = 192; i < 256; ++i) {
        char readChar = EEPROM.read(i);
        if (isPrintable(readChar) && readChar != '\0') {
            userPassword += readChar;
            PASSWORD = userPassword;
        }
    }

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
    // Write SSID to EEPROM with padding
    for (int i = 0; i < 32; ++i) {
        if (i < ssid.length()) {
            EEPROM.write(i, ssid[i]);
        } else {
            EEPROM.write(i, 0);  // Clear remaining space
        }
    }

    // Write password to EEPROM with padding
    for (int i = 0; i < 32; ++i) {  // Corrected to write within allocated space
        if (i < password.length()) {
            EEPROM.write(32 + i, password[i]);
        } else {
            EEPROM.write(32 + i, 0);  // Clear remaining space
        }
    }

    // Write email to EEPROM with padding
    for (int i = 0; i < 128; ++i) {  // Corrected to write within allocated space
        if (i < email.length()) {
            EEPROM.write(64 + i, email[i]);
        } else {
            EEPROM.write(64 + i, 0);  // Clear remaining space
        }
    }

    // Write user password to EEPROM with padding
    for (int i = 0; i < 64; ++i) {  // Corrected to write within allocated space
        if (i < userPassword.length()) {
            EEPROM.write(192 + i, userPassword[i]);
        } else {
            EEPROM.write(192 + i, 0);  // Clear remaining space
        }
    }

    // Commit changes and check for success
    if (EEPROM.commit()) {
        Serial.println("Credentials saved to EEPROM successfully!");
    } else {
        Serial.println("EEPROM commit failed.");
    }
}

void eraseCredentials() {
    // Clear EEPROM values
    for (int i = 0; i < 256; ++i) {
        EEPROM.write(i, 0);
    }

    // Commit changes and check for success
    if (EEPROM.commit()) {
        Serial.println("EEPROM cleared successfully!");
    } else {
        Serial.println("Failed to clear EEPROM.");
    }
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
            Serial.println("No valid Wi-Fi credentials found in EEPROM.");
            warnedNoCreds = true;
        }
        return false;
    }
}
