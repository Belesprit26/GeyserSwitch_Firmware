#include "interfaces/leak_detection.h"

// Water Leak sensor logic
void leakDetection() {
    digitalWrite(POWER_PIN, HIGH);
    delay(1000);
    int rain_value = analogRead(AO_PIN);
    digitalWrite(POWER_PIN, LOW);
    Serial.print("Leak Sensor reading: ");
    Serial.println(rain_value);
}
