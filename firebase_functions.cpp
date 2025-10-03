#include "firebase_functions.h"

// Enable Firebase features and include the library only in this translation unit
#ifndef ENABLE_DATABASE
#define ENABLE_DATABASE
#endif
#ifndef ENABLE_JWT
#define ENABLE_JWT
#endif
#ifndef ENABLE_USER_AUTH
#define ENABLE_USER_AUTH
#endif
#include <FirebaseClient.h>
using namespace firebase_ns;
#include "date_util.h"
#include "timers.h"
#include "credentials.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <cctype>  // For isalnum 

// Root CA and sensitive config are provided by secrets.h via firebase_functions.h include

// Global variable pointers for lazy initialization
static FirebaseApp *appPtr = nullptr;
static AsyncClientClass *aClientPtr = nullptr;
static RealtimeDatabase *DatabasePtr = nullptr;
static AsyncResult *aResult_no_callback_ptr = nullptr;
static WiFiClientSecure *ssl_client_ptr = nullptr; // Pointer for WiFiClientSecure

// Global variables
unsigned long previousMillis = 0;  // Tracks the last time something was triggered
const long interval = 10 * 1000;     

// Control variables
bool controlGeyser1 = false;
bool controlGeyser2 = false;
bool controlBothGeysers = false;

// Initialize Firebase path variables
String gsFree = "/GeyserSwitch"; // This will be updated with userId after fetching it
String userId = "";  // Initialize the userId variable
String gsPrime = "/GeyserSwitch_Prime"; 

// Timers
const String timer1 = "/Timers/04:00";
const String timer2 = "/Timers/06:00";
const String timer3 = "/Timers/08:00";
const String timer4 = "/Timers/16:00";
const String timer5 = "/Timers/18:00";
const String timer7 = "/Timers/CUSTOM";

// Geyser
const String geyser_1 = "/Geysers/geyser_1/state";

// Sensor
const String sensor_1 = "/Geysers/geyser_1/sensor_1";
const String sensor_1_max = "/Geysers/geyser_1/max_temp";

// Last Reboot
const String records = "/Records/LastBoot";

// Last Updated
const String updateRecords = "/Records/LastUpdate";

// Forward declarations for local helpers
static void printResult(AsyncResult &aResult);

// Define the rest of the functions, e.g., setupFirebase(), authHandler(), etc.
void setupFirebase()
{
    // (moved TLS configuration after client allocation below)

    Serial.println("Setting up Firebase...");

    // Read credentials from EEPROM
    String ssid, password, email, userPassword;
    readCredentials(ssid, password, email, userPassword);

    if (email.length() == 0 || userPassword.length() == 0) {
        Serial.println("Email or User Password not found in EEPROM. Cannot initialize Firebase.");
        sendNotification("Unit Issue:\nEmail or User Password not found in your unit. Cannot initialize Firebase. Reset the unit and enter your credentials again","Date: " + realDate + "\nTime: " + realTime);
        return;
    }

    // Create networking and app objects lazily
#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
    if (!ssl_client_ptr) ssl_client_ptr = new WiFiClientSecure();
#endif
    if (!aResult_no_callback_ptr) aResult_no_callback_ptr = new AsyncResult();
    if (!aClientPtr) aClientPtr = new AsyncClientClass(*ssl_client_ptr);
    if (!appPtr) appPtr = new FirebaseApp();
    if (!DatabasePtr) DatabasePtr = new RealtimeDatabase();

    // Configure TLS now that the SSL client exists
#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
    if (ssl_client_ptr) {
        // For quick validation you can switch to insecure, then revert to CA
        // ssl_client_ptr->setInsecure();
        ssl_client_ptr->setCACert(firebase_root_ca);
    }
#endif

    // Initialize Firebase Authentication with credentials from EEPROM
    UserAuth user_auth(API_KEY, email, userPassword);

    initializeApp(*aClientPtr, *appPtr, getAuth(user_auth), *aResult_no_callback_ptr);

    // Binding the FirebaseApp for authentication handler.
    appPtr->getApp<RealtimeDatabase>(*DatabasePtr);
    DatabasePtr->url(DATABASE_URL);

     // Setting the external async result to the sync task (optional)
    aClientPtr->setAsyncResult(*aResult_no_callback_ptr);

    Serial.println("Firebase setup complete.");

    // Do not read UID here; auth is non-blocking. UID will be read in the main loop.
}

// Authentication Handler with Timeout and Watchdog Prevention
void authHandler() {
    // Make non-blocking: single iteration step and return
    if (appPtr && appPtr->isInitialized() && !appPtr->ready()) {
        JWT.loop(appPtr->getAuth());
        if (aResult_no_callback_ptr) printResult(*aResult_no_callback_ptr);
    }
}

void authLoop() {
    authHandler();
    if (DatabasePtr) DatabasePtr->loop();
}

bool firebaseIsReady()
{
    return (appPtr && appPtr->isInitialized() && appPtr->ready());
}

// Helper function to validate Firebase UID format
bool isValidFirebaseUid(const String& uid) {
    // Firebase UIDs are typically 20-40 alphanumeric characters, no slashes
    if (uid.length() < 20 || uid.length() > 40) return false;
    for (char c : uid) {
        if (!isalnum(c)) return false;  // Only allow alphanumeric
    }
    return true;
}

// Initialize user-specific DB base path once auth is ready. Returns true when set.
bool firebaseInitUserPath()
{
    static bool pathSet = false;
    const int maxRetries = 3;

    if (pathSet) return true;
    if (!firebaseIsReady()) return false;

    // Retry logic for UID fetch with exponential backoff
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        userId = appPtr->getUid();

        if (userId.length() > 0 && isValidFirebaseUid(userId)) {
            // Valid UID: Construct path safely
            if (!gsFree.endsWith("/")) gsFree += "/";
            gsFree += userId;
            pathSet = true;
            Serial.println("User path initialized successfully: " + gsFree);
            return true;
        } else {
            Serial.printf("UID fetch attempt %d failed or invalid UID: '%s' (length: %d)\n",
                          attempt + 1, userId.c_str(), userId.length());
            if (attempt < maxRetries - 1) {
                delay(1000 * (attempt + 1));  // Exponential backoff: 1s, 2s, 3s
            }
        }
    }

    // All retries failed: Log critical error and prevent DB operations
    Serial.println("CRITICAL: Failed to initialize user path after " + String(maxRetries) + " retries. Firebase DB operations disabled until resolved.");
    // Optionally send notification here if needed
    return false;
}

static void printResult(AsyncResult &aResult)
{
    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }
}

void printError(int code, const String &msg)
{
    Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}

// Function implementations with custom paths and values

bool setIntValue(const String &path, int value)
{
    Serial.print("Set int at " + path + "... ");
    bool status = DatabasePtr->set<int>(*aClientPtr, path, value);
    if (status)
        Serial.println("ok");
    else
        printError(dbLastErrorCode(), dbLastErrorMessage());
    return status;
}

bool setBoolValue(const String &path, bool value)
{
    Serial.print("Set bool at " + path + "... ");
    bool status = DatabasePtr->set<bool>(*aClientPtr, path, value);
    if (status)
        Serial.println("ok");
    else
        printError(dbLastErrorCode(), dbLastErrorMessage());
    return status;
}

bool setFloatValue(const String &path, float value)
{
    Serial.print("Set float at " + path + "... ");
    bool status = DatabasePtr->set<number_t>(*aClientPtr, path, number_t(value, 2));
    if (status)
        Serial.println("ok");
    else
        printError(dbLastErrorCode(), dbLastErrorMessage());
    return status;
}

bool setDoubleValue(const String &path, double value)
{
    Serial.print("Set double at " + path + "... ");
    bool status = DatabasePtr->set<number_t>(*aClientPtr, path, number_t(value, 5));
    if (status)
        Serial.println("ok");
    else
        printError(dbLastErrorCode(), dbLastErrorMessage());
    return status;
}

bool setStringValue(const String &path, const String &value)
{
    Serial.print("Set String at " + path + "... ");
    bool status = DatabasePtr->set<String>(*aClientPtr, path, value);
    if (status)
        Serial.println("ok");
    else
        printError(dbLastErrorCode(), dbLastErrorMessage());
    return status;
}

// Light wrappers used outside this TU (keep Firebase types private to this file)
bool dbGetBool(const String &path) { return DatabasePtr->get<bool>(*aClientPtr, path); }
String dbGetString(const String &path) { return DatabasePtr->get<String>(*aClientPtr, path); }
int dbGetInt(const String &path) { return DatabasePtr->get<int>(*aClientPtr, path); }
float dbGetFloat(const String &path) { return DatabasePtr->get<float>(*aClientPtr, path); }
double dbGetDouble(const String &path) { return DatabasePtr->get<double>(*aClientPtr, path); }
void dbLoop() { if (DatabasePtr) DatabasePtr->loop(); }
int dbLastErrorCode() { return aClientPtr ? aClientPtr->lastError().code() : -1; }
String dbLastErrorMessage() { return aClientPtr ? aClientPtr->lastError().message() : String(""); }

// Function to delete a Firebase path
void deleteFirebasePath(const String &path)
{
    Serial.print("Delete path " + path + "... ");
    bool status = DatabasePtr->remove(*aClientPtr, path);
    if (status)
        Serial.println("ok");
    else
        printError(dbLastErrorCode(), dbLastErrorMessage());
}

// Function to print Firebase errors
void printErrors(int code, const String &message) {
    Serial.print("Firebase Error [");
    Serial.print(code);
    Serial.print("]: ");
    Serial.println(message);
}
//--------------------------------------------------------------------------------------------------------

// Helper function for boolean values
void setDefaultBoolValue(const String &path, bool defaultValue)
{
    bool value = DatabasePtr->get<bool>(*aClientPtr, path);

    if (dbLastErrorCode() != 0) {
        Serial.println("Path does not exist, setting default boolean value.");
        setBoolValue(path, defaultValue);
    } else if (value == false) {  // Check if value is false
        Serial.println("Path value is false, setting default boolean value.");
        setBoolValue(path, defaultValue);
    } else {
        Serial.println("Path exists, reading boolean value: " + String(value));
    }
}

// Helper function for string values
void setDefaultStringValue(const String &path, const String &defaultValue)
{
    String value = DatabasePtr->get<String>(*aClientPtr, path);

    if (dbLastErrorCode() != 0) {
        Serial.println("Path does not exist, setting default string value.");
        setStringValue(path, defaultValue);
    } else if (value == "" || value == "null") {  // Check if value is empty or "null"
        Serial.println("Path value is null or empty, setting default string value.");
        setStringValue(path, defaultValue);
    } else {
        Serial.println("Path exists, reading string value: " + value);
    }
}

// Helper function for float values
void setDefaultFloatValue(const String &path, float defaultValue)
{
    float value = DatabasePtr->get<float>(*aClientPtr, path);

    if (dbLastErrorCode() != 0) {
        Serial.println("Path does not exist, setting default float value.");
        setFloatValue(path, defaultValue);
    } else if (isnan(value) || value == 0.0f) {
        Serial.println("Path value is null or zero, setting default float value.");
        setFloatValue(path, defaultValue);
    } else {
        Serial.println("Path exists, reading float value: " + String(value));
    }
}

// Helper function for double values
void setDefaultDoubleValue(const String &path, double defaultValue)
{
    double value = DatabasePtr->get<double>(*aClientPtr, path);

    if (dbLastErrorCode() != 0) {
        Serial.println("Path does not exist, setting default double value.");
        setDoubleValue(path, defaultValue);
    } else if (isnan(value) || value == 0.0) {
        Serial.println("Path value is null or zero, setting default double value.");
        setDoubleValue(path, defaultValue);
    } else {
        Serial.println("Path exists, reading double value: " + String(value));
    }
}

///////=============================================================================================================================
// ------------------- Notification Function Implementations -------------------

// Function to send notification via Cloud Function
void sendNotification(String bodyMessage, String dataValue) {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        
        // Set the Root CA certificate for SSL verification
        client.setCACert(firebase_root_ca);

        HTTPClient https;

        // Cloud Function URL
        // It's already defined globally, so no need to redeclare here
        const char* cloudFunctionUrl = "https://us-central1-geyserswitch-bloc.cloudfunctions.net/sendNotificationFromESP32";
        const char* authKey = "geyserswitch-bloc-orange";  // Ensure this matches the key in your Cloud Function

        // Initialize HTTPS connection
        if (!https.begin(client, cloudFunctionUrl)) {
            Serial.println("Unable to connect to Cloud Function URL");
            return;
        }
        
        https.addHeader("Content-Type", "application/json");

        // Prepare JSON payload
        String payload = "{";
        payload += "\"title\":\"GeyserSwitch Alert\",";
        payload += "\"body\":\"" + bodyMessage + "\",";
        payload += "\"data\": \"" + dataValue + "\",";
        payload += "\"userId\":\"" + userId + "\",";
        payload += "\"authKey\":\"" + String(authKey) + "\"";
        payload += "}";
    

        

        // Send the POST request
        int httpResponseCode = https.POST(payload);

        // Handle the response
        if (httpResponseCode > 0) {
            String response = https.getString();
            Serial.println("Response Code: " + String(httpResponseCode));
            Serial.println("Response: " + response);
        } else {
            Serial.println("Error sending notification.");
            Serial.println("HTTP Response code: " + String(httpResponseCode));
        }

        // Close the HTTPS connection
        https.end();
    } else {
        Serial.println("Error in Wi-Fi connection");
    }
}

// Overloaded functions for different data types
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

// Callback function for Firebase real-time updates
void firebaseUpdateCallback(AsyncResult &aResult) {
    if (aResult.available()) {
        // Check if this is an update to the geyser control path
        String path = aResult.path();
        if (path == gsFree + geyser_1) {
            // Immediate response to Firebase changes
            bool geyserState = aResult.to<bool>();
            digitalWrite(geyser_1_pin, geyserState ? HIGH : LOW);
            Serial.printf("Geyser %s via Firebase (real-time)\n", geyserState ? "ON" : "OFF");
        }
    }
}

// Firebase real-time listener setup
void setupFirebaseListeners() {
    if (DatabasePtr && aClientPtr) {
        Serial.println("Setting up Firebase real-time listeners...");
        // Listen for changes to geyser control
        DatabasePtr->get(*aClientPtr, gsFree + geyser_1, firebaseUpdateCallback, true); // true = keep listening
        Serial.println("Firebase real-time listener active for geyser control");
    } else {
        Serial.println("Firebase not initialized - cannot setup listeners");
    }
}
