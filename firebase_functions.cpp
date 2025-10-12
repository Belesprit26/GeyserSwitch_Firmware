#include "interfaces/firebase_functions.h"
#include "interfaces/tracking.h"
#include "state/app_state.h"         // Centralized state management
#include "state/persistence.h"       // State persistence layer

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
#include "interfaces/date_util.h"
#include "interfaces/timers.h"
#include "interfaces/credentials.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "interfaces/notifications.h" 

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

// Initialize Firebase path variables - now managed by AppState
// String gsFree = "/GeyserSwitch"; // → AppState::getGsFree()
// String userId = "";              // → AppState::getUserId()
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

// Config cache moved to interfaces/config_cache.cpp

// Forward declarations for local helpers
static void printResult(AsyncResult &aResult);
static String firebaseHost();
static bool quickTlsProbe(const char* host);
// cache refresh implemented in interfaces/config_cache.cpp

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
        // Manual root CA pinning (compatible with ESP32 core 3.3.1)
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

    // Optional TLS probes for diagnostics
    quickTlsProbe(firebaseHost().c_str());
    quickTlsProbe("securetoken.googleapis.com");
    quickTlsProbe("identitytoolkit.googleapis.com");

    Serial.println("Firebase setup complete.");

    // Do not read UID here; auth is non-blocking. UID will be read in the main loop.
}

// Authentication Handler with Timeout and Watchdog Prevention
void authHandler() {
    // Make non-blocking: single iteration step and return
    if (appPtr && appPtr->isInitialized() && !appPtr->ready()) {
        JWT.loop(appPtr->getAuth());
        if (aResult_no_callback_ptr) printResult(*aResult_no_callback_ptr);
        // Auth visibility (rate-limited to 1s)
        static unsigned long _lastAuthLogMs = 0;
        unsigned long _now = millis();
        if (_now - _lastAuthLogMs >= 1000) {
            Serial.printf("Auth state - init:%d ready:%d code:%d\n",
                          appPtr->isInitialized(), appPtr->ready(),
                          aResult_no_callback_ptr ? aResult_no_callback_ptr->error().code() : 0);
            _lastAuthLogMs = _now;
        }
    }
}

void authLoop() {
    // Run auth processing continuously per library guidance
    authHandler();
    if (DatabasePtr) DatabasePtr->loop();

    // One-time banner when auth becomes ready
    static bool _authBannerPrinted = false;
    if (!_authBannerPrinted && firebaseIsReady()) {
        String uidStr = appPtr ? appPtr->getUid() : String("");
        Serial.println("================ FIREBASE AUTHENTICATED ================");
        Serial.printf("Host: %s\n", firebaseHost().c_str());
        Serial.printf("UID : %s\n", uidStr.c_str());
        Serial.println("=======================================================");
        _authBannerPrinted = true;
    }
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
        String uid = appPtr->getUid();
        AppState::setUserId(uid);

        if (uid.length() > 0 && isValidFirebaseUid(uid)) {
            // Valid UID: Construct path safely
            String gsFree = "/GeyserSwitch";
            if (!gsFree.endsWith("/")) gsFree += "/";
            gsFree += uid;
            AppState::setGsFree(gsFree);
            pathSet = true;
            Serial.println("User path initialized successfully: " + gsFree);
            // Ensure default tree exists once per boot after auth is ready
            ensureDefaultTreeIfMissing();
            return true;
        } else {
            Serial.printf("UID fetch attempt %d failed or invalid UID: '%s' (length: %d)\n",
                          attempt + 1, uid.c_str(), uid.length());
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

// Helper: extract Firebase host from DATABASE_URL for clearer error logs
static String firebaseHost()
{
    static String host;
    if (host.length() == 0) {
        String url = String(DATABASE_URL);
        if (url.startsWith("https://")) url.remove(0, 8);
        int slash = url.indexOf('/');
        if (slash >= 0) url = url.substring(0, slash);
        host = url;
    }
    return host;
}

// Helper: log detailed DB failure context
static void logDbFailure(const char* op, const String &path)
{
    Serial.printf("Firebase %s failed: host=%s path=%s code=%d msg:%s\n",
                  op,
                  firebaseHost().c_str(),
                  path.c_str(),
                  dbLastErrorCode(),
                  dbLastErrorMessage().c_str());
}

// Quick TLS connectivity probe to a given host:443 using current root CA
static bool quickTlsProbe(const char* host)
{
    WiFiClientSecure p;
    // Use the same trust mechanism as main client (manual CA pinning)
    p.setCACert(firebase_root_ca);
    // Short timeout to avoid blocking long
    p.setTimeout(5000);
    Serial.printf("TLS probe: connecting to https://%s:443 ... ", host);
    bool ok = p.connect(host, 443);
    if (ok) {
        Serial.println("ok");
        p.stop();
        return true;
    } else {
        Serial.println("FAILED");
        return false;
    }
}

bool setIntValue(const String &path, int value)
{
    Serial.print("Set int at " + path + "... ");
    bool status = DatabasePtr->set<int>(*aClientPtr, path, value);
    if (status)
        Serial.println("ok");
    else
    {
        printError(dbLastErrorCode(), dbLastErrorMessage());
        logDbFailure("set<int>", path);
    }
    return status;
}

bool setBoolValue(const String &path, bool value)
{
    Serial.print("Set bool at " + path + "... ");
    bool status = DatabasePtr->set<bool>(*aClientPtr, path, value);
    if (status)
        Serial.println("ok");
    else
    {
        printError(dbLastErrorCode(), dbLastErrorMessage());
        logDbFailure("set<bool>", path);
    }
    return status;
}

bool setFloatValue(const String &path, float value)
{
    Serial.print("Set float at " + path + "... ");
    bool status = DatabasePtr->set<number_t>(*aClientPtr, path, number_t(value, 2));
    if (status)
        Serial.println("ok");
    else
    {
        printError(dbLastErrorCode(), dbLastErrorMessage());
        logDbFailure("set<float>", path);
    }
    return status;
}

bool setDoubleValue(const String &path, double value)
{
    Serial.print("Set double at " + path + "... ");
    bool status = DatabasePtr->set<number_t>(*aClientPtr, path, number_t(value, 5));
    if (status)
        Serial.println("ok");
    else
    {
        printError(dbLastErrorCode(), dbLastErrorMessage());
        logDbFailure("set<double>", path);
    }
    return status;
}

bool setStringValue(const String &path, const String &value)
{
    Serial.print("Set String at " + path + "... ");
    bool status = DatabasePtr->set<String>(*aClientPtr, path, value);
    if (status)
        Serial.println("ok");
    else
    {
        printError(dbLastErrorCode(), dbLastErrorMessage());
        logDbFailure("set<String>", path);
    }
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
    {
        printError(dbLastErrorCode(), dbLastErrorMessage());
        logDbFailure("remove", path);
    }
}

// Function to print Firebase errors
void printErrors(int code, const String &message) {
    Serial.print("Firebase Error [");
    Serial.print(code);
    Serial.print("]: ");
    Serial.println(message);
}
//--------------------------------------------------------------------------------------------------------

// ensureDefaultTreeIfMissing, refreshConfigCache10s and getters are implemented in interfaces/config_cache.cpp

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
// ------------------- Notification wrappers moved to notifications.{h,cpp} -------------------

// Callback function for Firebase real-time updates
void firebaseUpdateCallback(AsyncResult &aResult) {
    if (aResult.available() && DatabasePtr && aClientPtr) {  // Safety check for initialized pointers
        // Check if this is an update to the geyser control path
        String path = aResult.path();
        if (path == AppState::getGsFree() + geyser_1) {
            // Immediate response to Firebase changes - use same approach as dbGetBool
            bool geyserState = DatabasePtr->get<bool>(*aClientPtr, path);
            digitalWrite(15, geyserState ? HIGH : LOW);  // TODO: Use geyser_1_pin variable
            AppState::setGeyserOn(geyserState);  // Update centralized state
            Serial.printf("Geyser %s via Firebase (real-time)\n", geyserState ? "ON" : "OFF");

            // Track geyser usage for duration logging
            trackGeyserUsage(geyser_1, geyserState);
        }
    }
}

// Firebase real-time listener setup
void setupFirebaseListeners() {
    if (DatabasePtr && aClientPtr) {
        Serial.println("Setting up Firebase real-time listeners...");
        // Listen for changes to geyser control
        DatabasePtr->get(*aClientPtr, AppState::getGsFree() + geyser_1, firebaseUpdateCallback, true); // true = keep listening
        Serial.println("Firebase real-time listener active for geyser control");
    } else {
        Serial.println("Firebase not initialized - cannot setup listeners");
    }
}
