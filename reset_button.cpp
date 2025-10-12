#include "interfaces/reset_button.h"
#include "interfaces/credentials.h"

// Static variables for button state (persistent across calls)
static bool isButtonPressed = false;
static unsigned long buttonPressTime = 0;
static bool messagePrinted = false;

// Function to Check Reset Button Press
void checkResetButton(int rstButtonPin) {
    if (digitalRead(rstButtonPin) == LOW) {  // Button is pressed
        if (!isButtonPressed) {
            buttonPressTime = millis();
            isButtonPressed = true;
            messagePrinted = false;  // Reset the message printed flag
        } else {
            unsigned long elapsed = millis() - buttonPressTime;
            if (elapsed >= 1000 && !messagePrinted) {
                Serial.println("This unit is about to clear the EEPROM");
                messagePrinted = true;
            }
            if (elapsed >= 3000) {
                eraseCredentials();  // Erase Wi-Fi credentials from EEPROM
                Serial.println("Credentials erased!");
                isButtonPressed = false;
                messagePrinted = false;  // Reset the flag
                ESP.restart();  // Restart the ESP32
            }
        }
    } else {  // Button is not pressed
        if (isButtonPressed && messagePrinted) {
            // If the button was released before 3 seconds after the warning
            Serial.println("Button released before clearing EEPROM.");
        }
        isButtonPressed = false;   // Reset the button press flag
        messagePrinted = false;    // Reset the message printed flag
    }
}
