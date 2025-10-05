#include <Arduino.h>
#include "date_util.h"
#include "firebase_functions.h"
#include "temp_control.h"
#include "timers.h" 
#include "ota.h" 
#include "tracking.h"
#include "secrets.h"
#include "leak_detection.h"
#include "reset_button.h"
#include <PubSubClient.h>  // For MQTT broker/client functionality (legacy include)
#include "mqtt.h"         // Consolidated MQTT module

//----------------------------------------WEBSERVER----------------------------------------------------------
#include "credentials.h"
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



// Runtime flags
static bool wifiConnected = false;
static bool firebaseStarted = false;
static bool postFirebaseInitDone = false;

// FreeRTOS Task Handles
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t controlTaskHandle = NULL;
TaskHandle_t syncTaskHandle = NULL;

// FreeRTOS Mutex for protecting shared resources
SemaphoreHandle_t controlMutex = NULL;

// MQTT now handled in mqtt.{h,cpp}

// Offline Queue Structure (for storing commands/states)
struct OfflineQueue {
    String commands[10];  // Simple array for queued MQTT commands
    int head = 0;
    int tail = 0;
    bool isFull() { return (tail + 1) % 10 == head; }
    bool isEmpty() { return head == tail; }
    void enqueue(String cmd) { if (!isFull()) { commands[tail] = cmd; tail = (tail + 1) % 10; } }
    String dequeue() { if (!isEmpty()) { String cmd = commands[head]; head = (head + 1) % 10; return cmd; } return ""; }
};
OfflineQueue offlineQueue;


void setup() {
    Serial.begin(115200);
    delay(500);
    unsigned long _t = millis();
    while ((millis() - _t) < 1500 && !Serial) {
        delay(10);
    }
    Serial.println("Boot start");
    pinMode(rstButtonPin, INPUT_PULLUP);  // Setup the reset button with pull-up resistor

    // Early LED setup for visibility
    pinMode(geyser_1_pin, OUTPUT);
    digitalWrite(geyser_1_pin, LOW);
    delay(50);
    digitalWrite(geyser_1_pin, HIGH);
    delay(50);
    digitalWrite(geyser_1_pin, LOW);

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

    // Initialize consolidated MQTT module (AP + broker)
    mqttns::mqttInit();

    // Start FreeRTOS Tasks
    xTaskCreate(mqttTask, "MQTT Task", 4096, NULL, 1, &mqttTaskHandle);       // Priority 1: Medium (communications)
    xTaskCreate(controlTask, "Control Task", 8192, NULL, 2, &controlTaskHandle); // Priority 2: High (critical control)
    xTaskCreate(syncTask, "Sync Task", 8192, NULL, 0, &syncTaskHandle);         // Priority 0: Low (background sync) - Increased stack for Firebase operations
}

void loop() {
    // Simplified loop: Core logic now in FreeRTOS tasks
    checkResetButton(rstButtonPin);  // Reset button check
    server.handleClient();           // Web server for config

    // Option 1: Ensure Firebase auth processing runs continuously at high cadence
    authLoop();
    
    // Wi-Fi STA connection (for Firebase/MQTT app sync)
    if (!wifiConnected) {
        wifiConnected = checkAndConnectWiFi(server, 1500);
        if (wifiConnected) {
            Serial.println("Wi-Fi connected - switching to STA mode");
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            // Firebase init moved to sync task
        }
    }

    // If no STA, keep AP for local MQTT/config
    if (!wifiConnected) {
        static bool apStarted = false;
        if (!apStarted) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_AP);
            WiFi.softAP("GeyserHub", "password123");  // MQTT broker AP
            setupWebServer();
            Serial.println("AP mode active for MQTT/config.");
            apStarted = true;
        }
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Yield to tasks
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

// MQTT Task: Handle broker/client operations
void mqttTask(void *pvParameters) {
    for (;;) {
        mqttns::mqttBrokerTaskLoop();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void controlTask(void *pvParameters) {
    // Handle sensor reading, geyser control, and MQTT commands
    for (;;) {
        // Process queued MQTT commands (protected by mutex)
        if (xSemaphoreTake(controlMutex, portMAX_DELAY) == pdTRUE) {
            // Consume MQTT commands from central queue
            {
                String cmd = mqttns::mqttDequeueCommand();
                if (cmd == "on") {
                    digitalWrite(15, HIGH);  // Use hardcoded pin
                    Serial.println("Geyser turned ON via MQTT");
                } else if (cmd == "off") {
                    digitalWrite(15, LOW);  // Use hardcoded pin
                    Serial.println("Geyser turned OFF via MQTT");
                }
            }

            // Existing sensor-based control (includes OneWire operations)
            controlGeyserBasedOnMaxTemp();

            xSemaphoreGive(controlMutex);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);  // 5s loop
    }
}

void syncTask(void *pvParameters) {
    // Handle offline/online detection and Firebase sync
    static bool wasOnline = false;
    static bool firebaseInitialized = false;
    static unsigned long lastSyncTime = 0;
    const unsigned long syncInterval = 10000;  // 10 seconds sync interval for config refresh

    for (;;) {
        // Check if Wi-Fi is connected
        bool wifiConnected = (WiFi.status() == WL_CONNECTED);

        // Initialize Firebase when Wi-Fi connects (only once)
        if (wifiConnected && !firebaseInitialized) {
            Serial.println("Wi-Fi connected - initializing Firebase...");
            // Ensure NTP time is synced before any TLS/Firebase operations
            initializeAndSyncTime(realDate, realTime);
            setupFirebase();
            firebaseInitialized = true;
            Serial.println("Firebase initialization attempted");
        }

        // Process Firebase authentication (required for async auth to complete)
        if (firebaseInitialized) {
            authLoop();  // This handles the authentication process
        }

        bool isOnline = (wifiConnected && firebaseIsReady());

        // Ensure per-user base path is initialized once auth is ready
        static bool userPathInit = false;
        if (wifiConnected && firebaseIsReady() && !userPathInit) {
            if (firebaseInitUserPath()) {
                userPathInit = true;
            }
        }

        if (isOnline && userPathInit && !wasOnline) {
            Serial.println("Back online - syncing queued data to Firebase");
            // Sync current geyser state immediately
            bool currentState = digitalRead(geyser_1_pin) == HIGH;
            if (setBoolValue(gsFree + geyser_1, currentState)) {
                Serial.println("Geyser state synced successfully");
            } else {
                Serial.println("Failed to sync geyser state - will retry");
            }
            // Setup real-time Firebase listeners for immediate control
            setupFirebaseListeners();
            // TODO: Sync any queued sensor data or MQTT history here
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
    }
}
