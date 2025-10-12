#include "interfaces/notifications.h"
#include "interfaces/secrets.h"
#include "interfaces/firebase_functions.h" // for firebaseIsReady(), userId
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// Centralized notification sender using Cloud Functions (HTTPS)
// Requires: Wi-Fi connected, valid firebase_root_ca in secrets.h

static const char* kCloudFunctionUrl = "https://us-central1-geyserswitch-bloc.cloudfunctions.net/sendNotificationFromESP32";
static const char* kAuthKey = "geyserswitch-bloc-orange";

void sendNotification(String bodyMessage, String dataValue) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Error in Wi-Fi connection");
        return;
    }

    WiFiClientSecure client;
    client.setCACert(firebase_root_ca);

    HTTPClient https;
    if (!https.begin(client, kCloudFunctionUrl)) {
        Serial.println("Unable to connect to Cloud Function URL");
        return;
    }

    https.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"title\":\"GeyserSwitch Alert\",";
    payload += "\"body\":\"" + bodyMessage + "\",";
    payload += "\"data\": \"" + dataValue + "\",";
    payload += "\"userId\":\"" + (firebaseIsReady() ? userId : String("")) + "\",";
    payload += "\"authKey\":\"" + String(kAuthKey) + "\"";
    payload += "}";

    int httpResponseCode = https.POST(payload);
    if (httpResponseCode > 0) {
        String response = https.getString();
        Serial.println("Response Code: " + String(httpResponseCode));
        Serial.println("Response: " + response);
    } else {
        Serial.println("Error sending notification.");
        Serial.println("HTTP Response code: " + String(httpResponseCode));
    }
    https.end();
}

void sendNotificationString(String bodyMessage, String dataValue) {
    sendNotification(bodyMessage, dataValue);
}

void sendNotificationInt(String bodyMessage, int dataValue) {
    sendNotification(bodyMessage, String(dataValue));
}

void sendNotificationFloat(String bodyMessage, float dataValue) {
    sendNotification(bodyMessage, String(dataValue));
}

void sendNotificationJSON(String bodyMessage, String jsonData) {
    sendNotification(bodyMessage, jsonData);
}


