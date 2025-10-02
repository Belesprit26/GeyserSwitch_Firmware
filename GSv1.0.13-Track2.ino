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
#include <PubSubClient.h>  // For MQTT broker/client functionality

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

// MQTT Client and Server (for broker emulation)
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiServer mqttServer(1883);  // MQTT broker on port 1883

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

    // Set up MQTT Broker (ESP32 as AP)
    WiFi.softAP("GeyserHub", "password123");  // Create AP for clients to connect
    mqttServer.begin();  // Start MQTT broker server
    Serial.println("MQTT Broker started on ESP32 AP");

    // Start FreeRTOS Tasks
    xTaskCreate(mqttTask, "MQTT Task", 4096, NULL, 1, &mqttTaskHandle);       // Priority 1: Medium (communications)
    xTaskCreate(controlTask, "Control Task", 4096, NULL, 2, &controlTaskHandle); // Priority 2: High (critical control)
    xTaskCreate(syncTask, "Sync Task", 4096, NULL, 0, &syncTaskHandle);         // Priority 0: Low (background sync)
}

void loop() {
    // Simplified loop: Core logic now in FreeRTOS tasks
    checkResetButton(rstButtonPin);  // Reset button check
    server.handleClient();           // Web server for config
    
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
        // Handle incoming MQTT broker clients
        WiFiClient client = mqttServer.available();
        if (client) {
            Serial.println("New MQTT client connected");
            // Basic MQTT broker emulation: accept connections and handle simple messages
            while (client.connected()) {
                if (client.available()) {
                    String msg = client.readStringUntil('\n');
                    if (msg.startsWith("PUBLISH geyser/cmd")) {
                        // Extract command (e.g., "on" or "off")
                        String cmd = msg.substring(msg.lastIndexOf(' ') + 1);
                        offlineQueue.enqueue(cmd);  // Queue for control task
                        Serial.println("MQTT Command Queued: " + cmd);
                    }
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            client.stop();
            Serial.println("MQTT client disconnected");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 1s loop
    }
}

void controlTask(void *pvParameters) {
    // Handle sensor reading, geyser control, and MQTT commands
    for (;;) {
        // Process queued MQTT commands
        if (!offlineQueue.isEmpty()) {
            String cmd = offlineQueue.dequeue();
            if (cmd == "on") {
                digitalWrite(geyser_1_pin, HIGH);
                Serial.println("Geyser turned ON via MQTT");
            } else if (cmd == "off") {
                digitalWrite(geyser_1_pin, LOW);
                Serial.println("Geyser turned OFF via MQTT");
            }
        }

        // Existing sensor-based control
        controlGeyserBasedOnMaxTemp();

        vTaskDelay(5000 / portTICK_PERIOD_MS);  // 5s loop
    }
}

void syncTask(void *pvParameters) {
    // Handle offline/online detection and Firebase sync
    static bool wasOnline = false;
    static unsigned long lastSyncTime = 0;
    const unsigned long syncInterval = 60000;  // 1 minute sync interval

    for (;;) {
        bool isOnline = (WiFi.status() == WL_CONNECTED && firebaseIsReady());

        if (isOnline && !wasOnline) {
            Serial.println("Back online - syncing queued data to Firebase");
            // Sync current geyser state immediately
            bool currentState = digitalRead(geyser_1_pin) == HIGH;
            if (setBoolValue(gsFree + geyser_1, currentState)) {
                Serial.println("Geyser state synced successfully");
            } else {
                Serial.println("Failed to sync geyser state - will retry");
            }
            // TODO: Sync any queued sensor data or MQTT history here
            wasOnline = true;
            lastSyncTime = millis();
        } else if (!isOnline && wasOnline) {
            Serial.println("Gone offline - operating locally, data will queue");
            wasOnline = false;
        }

        // Periodic Firebase sync if online
        if (isOnline && millis() - lastSyncTime >= syncInterval) {
            Serial.println("Performing periodic Firebase sync");
            // Sync time and update records
            initializeAndSyncTime(realDate, realTime);
            if (setStringValue(gsFree + updateRecords + "/updateDate", realDate) &&
                setStringValue(gsFree + updateRecords + "/updateTime", realTime)) {
                Serial.println("Time and records synced successfully");
            } else {
                Serial.println("Failed to sync time/records - will retry next cycle");
            }
            lastSyncTime = millis();
        }

        vTaskDelay(10000 / portTICK_PERIOD_MS);  // 10s loop
    }
}
