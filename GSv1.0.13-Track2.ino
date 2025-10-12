#include <Arduino.h>
#include "interfaces/date_util.h"
#include "interfaces/firebase_functions.h"
#include "interfaces/temp_control.h"
#include "interfaces/timers.h" 
#include "interfaces/ota.h" 
#include "interfaces/tracking.h"
#include "interfaces/secrets.h"
#include "interfaces/leak_detection.h"
#include "interfaces/reset_button.h"
// MQTT module removed - using Firebase-only communication

//----------------------------------------WEBSERVER----------------------------------------------------------
#include "interfaces/credentials.h"
#include <WebServer.h>

// Constants for Reset Button
const int rstButtonPin = 9;  // GPIO pin for the reset button (moved off strap pin)

// Web Server on Port 80
WebServer server(80);

// Function Prototypes
void handleRoot();
void handleSetting();
void setupWebServer();
//--------------------------------------------------------------------------------------------------

/// Variables
#if defined(ESP8266)
    // Pin assignment for ESP8266
    const int geyser_1_pin = 13; // geyser_1 pin for ESP8266
#elif defined(ESP32)
    // Pin assignment for ESP32 / ESP32-C6
    const int geyser_1_pin = 15; // 23 is correct. TEST: use onboard LED (GPIO 15) for geyser output
#else
    #error "Unsupported platform"
#endif


// Declare the timer function prototype
void controlGeyserWithTimers();


// Setup temp declaration
void setupTempSensor();



// Runtime flags - now managed by AppState
// Runtime state variables now managed locally in each task

// FreeRTOS Task Handles
// MQTT task handle removed - MQTT functionality removed
TaskHandle_t controlTaskHandle = NULL;
TaskHandle_t syncTaskHandle = NULL;

// FreeRTOS Mutex for protecting shared resources
SemaphoreHandle_t controlMutex = NULL;

// MQTT now handled in mqtt.{h,cpp}

// OfflineQueue removed - using mqtt.h CommandQueue instead


void setup() {
    Serial.begin(115200);
    delay(500);
    unsigned long _t = millis();
    while ((millis() - _t) < 1500 && !Serial) {
        delay(10);
    }
    Serial.println("Boot start");
    pinMode(rstButtonPin, INPUT_PULLUP);  // Setup the reset button with pull-up resistor

    // Set the geyser pins as outputs
    pinMode(geyser_1_pin, OUTPUT);
    digitalWrite(geyser_1_pin, LOW);  // Start with geyser off


    // Initialize EEPROM and tracking
    initEEPROM();
#if USE_TEMP_CREDS
    // Inject temporary credentials into EEPROM on first boot each flash
    saveCredentials(String(TEMP_SSID), String(TEMP_PASSWORD), String(TEMP_EMAIL), String(TEMP_USER_PASSWORD));
#endif
    initTracking();

    Serial.println("Booting...");

    // Initialize FreeRTOS Mutex for thread safety
    controlMutex = xSemaphoreCreateMutex();
    if (controlMutex == NULL) {
        Serial.println("ERROR: Failed to create mutex!");
        ESP.restart();
    }

    // MQTT module removed - using Firebase-only communication

    // Start FreeRTOS Tasks (MQTT task removed)
    xTaskCreate(controlTask, "Control Task", 8192, NULL, 2, &controlTaskHandle); // Priority 2: High (critical control)
    xTaskCreate(syncTask, "Sync Task", 8192, NULL, 1, &syncTaskHandle);         // Priority 1: Medium (background sync) - Increased stack for Firebase operations
    
    Serial.println("System initialization complete");
}

void loop() {
    // Simplified loop: Core logic now in FreeRTOS tasks
    checkResetButton(rstButtonPin);  // Reset button check
    server.handleClient();           // Web server for config

    // Firebase auth processing handled in syncTask only (no duplicate processing)
    
    // WiFi connection handled in syncTask - no AP mode needed (MQTT removed)

    vTaskDelay(100 / portTICK_PERIOD_MS);  // 100ms yield for better responsiveness
}

// Handler for Root Endpoint
void handleRoot() {
    String content = "<html><body><h1>ESP32 WiFi Config</h1><form action='/setting' method='POST'>";
    content += "<label>SSID: </label><input name='ssid' length=32><br><br>";
    content += "<label>Password: </label><input name='pass' length=64><br><br>";
    content += "<label>Email: </label><input name='email' length=128><br><br>";
    content += "<label>User Password: </label><input name='pass2' length=64><br><br>";
    content += "<input type='submit' value='Save'></form></body></html>";
    server.send(200, "text/html", content);
}

// Handler for Setting Credentials
void handleSetting() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String email = server.arg("email");
    String pass2 = server.arg("pass2");

    if (ssid.length() > 0 && pass.length() > 0 && email.length() > 0 && pass2.length() > 0) {
        // Save the settings to EEPROM
        saveCredentials(ssid, pass, email, pass2);
        server.send(200, "text/html", "<html><body>Settings Saved. Rebooting...</body></html>");
        delay(1000);
        ESP.restart();
    } else {
        server.send(400, "text/html", "<html><body>Missing data!</body></html>");
    }
}

// Function to set up the web server routes
void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/setting", handleSetting);
    Serial.println("Web server routes configured");
}

// MQTT Task removed - using Firebase-only communication

void controlTask(void *pvParameters) {
    // Handle sensor reading and geyser control (MQTT removed - using Firebase-only)
    for (;;) {
        // Sensor-based control (includes OneWire operations)
        if (xSemaphoreTake(controlMutex, portMAX_DELAY) == pdTRUE) {
            controlGeyserBasedOnMaxTemp();
            xSemaphoreGive(controlMutex);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);  // 5s loop
    }
}

void syncTask(void *pvParameters) {
    // Handle offline/online detection and Firebase sync
    static bool wifiInitAttempted = false;
    static bool wasOnline = false;
    static bool firebaseInitialized = false;
    static unsigned long lastSyncTime = 0;
    const unsigned long syncInterval = 10000;  // 10 seconds sync interval for config refresh

    for (;;) {
        // Initialize WiFi connection (once)
        if (!wifiInitAttempted) {
            Serial.println("=== WiFi Initialization ===");
            String ssid, password, email, userPassword;
            readCredentials(ssid, password, email, userPassword);
            
            if (ssid.length() > 0 && password.length() > 0) {
                Serial.printf("Connecting to WiFi: %s\n", ssid.c_str());
                WiFi.mode(WIFI_STA);
                WiFi.begin(ssid.c_str(), password.c_str());
                
                // Wait up to 10 seconds for connection
                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                    delay(500);
                    Serial.print(".");
                    attempts++;
                }
                Serial.println();
                
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.println("✓ WiFi connected!");
                    Serial.printf("  IP address: %s\n", WiFi.localIP().toString().c_str());
                } else {
                    Serial.println("✗ WiFi connection failed - will retry");
                }
        } else {
                Serial.println("✗ No WiFi credentials found");
            }
            wifiInitAttempted = true;
        }
        
        // Check WiFi status continuously
        bool wifiConnected = (WiFi.status() == WL_CONNECTED);
        
        // Reconnect WiFi if disconnected
        if (!wifiConnected && wifiInitAttempted) {
            static unsigned long lastReconnectAttempt = 0;
            if (millis() - lastReconnectAttempt >= 30000) {  // Try every 30 seconds
                Serial.println("WiFi disconnected - attempting reconnect...");
                String ssid, password, email, userPassword;
                readCredentials(ssid, password, email, userPassword);
                WiFi.begin(ssid.c_str(), password.c_str());
                lastReconnectAttempt = millis();
            }
        }

        // Initialize Firebase when Wi-Fi connects (only once)
        if (wifiConnected && !firebaseInitialized) {
            Serial.println("=== Firebase Initialization ===");
            Serial.println("Wi-Fi connected - initializing Firebase...");
            // Ensure NTP time is synced before any TLS/Firebase operations
            initializeAndSyncTime(realDate, realTime);
            setupFirebase();
            firebaseInitialized = true;
            Serial.println("Firebase initialization attempted");
        }

        // Process Firebase authentication (only during auth phase)
        if (firebaseInitialized && !firebaseIsReady()) {
            authLoop();  // Only for authentication process
        }
        
        // Process Firebase real-time listeners (continuously when ready)
        if (firebaseIsReady()) {
            dbLoop();  // Process real-time callbacks
            
            // Debug: Confirm Firebase loop is running (rate limited to once per 30 seconds)
            static unsigned long lastFirebaseLoopLog = 0;
            if (millis() - lastFirebaseLoopLog >= 30000) {
                Serial.println("📡 Firebase real-time loop active (listeners processing)");
                lastFirebaseLoopLog = millis();
            }
        }

        bool isOnline = (wifiConnected && firebaseIsReady());

        // Ensure per-user base path is initialized once auth is ready
        static bool userPathInit = false;
        if (wifiConnected && firebaseIsReady() && !userPathInit) {
            if (firebaseInitUserPath()) {
                userPathInit = true;
                Serial.println("User path: Ready!");
            }
        }
        
        // Initialize default tree asynchronously after user path is set
        if (userPathInit) {
            asyncInitializeDefaults();
        }

        if (isOnline && userPathInit && !wasOnline) {
            Serial.println("Back online - reading current state from Firebase");
            
            // Read current geyser state from Firebase FIRST
            String geyserPath = gsFree + geyser_1;
            Serial.printf("Reading geyser state from: %s\n", geyserPath.c_str());
            bool firebaseState = dbGetBool(geyserPath);
            
            if (dbLastErrorCode() == 0) {
                Serial.printf("Firebase geyser state: %s\n", firebaseState ? "ON" : "OFF");
                Serial.printf("🚀 INITIAL SYNC: Setting geyser to %s from Firebase\n", firebaseState ? "ON" : "OFF");
                digitalWrite(geyser_1_pin, firebaseState ? HIGH : LOW);
                Serial.println("Local state synced with Firebase");
            } else {
                // Only write to Firebase if path doesn't exist
                Serial.println("Firebase path not found - initializing with local state");
                bool currentState = (digitalRead(geyser_1_pin) == HIGH);
                if (setBoolValue(geyserPath, currentState)) {
                    Serial.println("Local state written to Firebase");
                } else {
                    Serial.println("Failed to write local state to Firebase");
                }
            }
            
            // Setup real-time Firebase listeners for immediate control
            setupFirebaseListeners();
            wasOnline = true;
            lastSyncTime = millis();
        } else if (!isOnline && wasOnline) {
            Serial.println("Gone offline - operating locally, data will queue");
            wasOnline = false;
        }

        // Periodic Firebase sync if online (sensors, logging - geyser control now uses real-time listeners)
        if (isOnline && millis() - lastSyncTime >= syncInterval) {
            // Protect Firebase operations with mutex
            if (xSemaphoreTake(controlMutex, portMAX_DELAY) == pdTRUE) {
                Serial.println("Performing periodic Firebase sync");
                // Sync time and update records (no geyser polling needed - listeners handle it)
                initializeAndSyncTime(realDate, realTime);
                if (setStringValue(gsFree + updateRecords + "/updateDate", realDate) &&
                    setStringValue(gsFree + updateRecords + "/updateTime", realTime)) {
                    Serial.println("Time and records synced successfully");
                } else {
                    Serial.println("Failed to sync time/records - will retry next cycle");
                }
                lastSyncTime = millis();
                xSemaphoreGive(controlMutex);
            }
        }

        // Check timers every sync interval (1 minute) when online
        static unsigned long lastTimerCheck = 0;
        if (isOnline && millis() - lastTimerCheck >= syncInterval) {
            // Refresh config cache OUTSIDE the mutex to avoid blocking other tasks
            refreshConfigCache10s();
            if (xSemaphoreTake(controlMutex, portMAX_DELAY) == pdTRUE) {
                Serial.println("Checking timer schedules...");
                controlGeyserWithTimers();
                lastTimerCheck = millis();
                xSemaphoreGive(controlMutex);
            }
        }
        
        // Add heartbeat every 30 seconds
        static unsigned long lastHeartbeat = 0;
        if (millis() - lastHeartbeat >= 30000) {
            Serial.printf("[%lu] System alive - WiFi:%d Firebase:%d Geyser:%d\n",
                          millis()/1000, wifiConnected, isOnline, (digitalRead(geyser_1_pin) == HIGH));
            lastHeartbeat = millis();
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 1s cycle to prevent CPU hogging
    }
}

