#include "persistence.h"
#include "interfaces/credentials.h"  // For EEPROM migration

namespace Persistence {
    Preferences credentialsPrefs;
    Preferences runtimePrefs;
    
    void init() {
        credentialsPrefs.begin("creds", false);
        runtimePrefs.begin("runtime", false);
        Serial.println("Persistence system initialized");
    }
    
    // Critical state (immediate save - must survive power loss)
    void saveGeyserState(bool on) {
        runtimePrefs.putBool("geyserOn", on);
        Serial.println("CRITICAL: Saved geyser state: " + String(on));
    }
    
    void saveTimerFlags(bool flags[5]) {
        uint8_t flagsByte = 0;
        for (int i = 0; i < 5; i++) {
            if (flags[i]) flagsByte |= (1 << i);
        }
        runtimePrefs.putUChar("timerFlags", flagsByte);
        Serial.println("CRITICAL: Saved timer flags");
    }
    
    void saveCustomTimerFlag(bool triggered) {
        runtimePrefs.putBool("customTimerFlag", triggered);
        Serial.println("CRITICAL: Saved custom timer flag: " + String(triggered));
    }
    
    // Load critical state
    bool loadGeyserState() {
        return runtimePrefs.getBool("geyserOn", false);
    }
    
    void loadTimerFlags(bool flags[5]) {
        uint8_t flagsByte = runtimePrefs.getUChar("timerFlags", 0);
        for (int i = 0; i < 5; i++) {
            flags[i] = (flagsByte & (1 << i)) != 0;
        }
    }
    
    bool loadCustomTimerFlag() {
        return runtimePrefs.getBool("customTimerFlag", false);
    }
    
    // Batch operations
    void saveAllState() {
        xSemaphoreTake(AppState::mutex, portMAX_DELAY);
        
        // Critical state (immediate save - must survive power loss)
        saveGeyserState(AppState::current.control.geyserOn);
        saveTimerFlags(AppState::current.control.timerFlags);
        saveCustomTimerFlag(AppState::current.control.customTimerFlag);
        
        // Non-critical state (batch save)
        runtimePrefs.putDouble("temperature1", AppState::current.hardware.temperature1);
        runtimePrefs.putString("userId", AppState::current.network.userId);
        runtimePrefs.putString("gsFree", AppState::current.network.gsFree);
        runtimePrefs.putString("realTime", AppState::current.control.realTime);
        runtimePrefs.putString("realDate", AppState::current.control.realDate);
        
        xSemaphoreGive(AppState::mutex);
        Serial.println("Saved all state to NVS");
    }
    
    void loadAllState() {
        xSemaphoreTake(AppState::mutex, portMAX_DELAY);
        
        // Critical state (must survive power loss)
        AppState::current.control.geyserOn = loadGeyserState();
        loadTimerFlags(AppState::current.control.timerFlags);
        AppState::current.control.customTimerFlag = loadCustomTimerFlag();
        
        // Non-critical state
        AppState::current.hardware.temperature1 = runtimePrefs.getDouble("temperature1", 0.0);
        AppState::current.network.userId = runtimePrefs.getString("userId", "");
        AppState::current.network.gsFree = runtimePrefs.getString("gsFree", "/GeyserSwitch");
        AppState::current.control.realTime = runtimePrefs.getString("realTime", "");
        AppState::current.control.realDate = runtimePrefs.getString("realDate", "");
        
        xSemaphoreGive(AppState::mutex);
        Serial.println("Loaded all state from NVS");
    }
    
    void saveNonCriticalState() {
        xSemaphoreTake(AppState::mutex, portMAX_DELAY);
        
        // Non-critical state (batch save every 30s)
        runtimePrefs.putDouble("temperature1", AppState::current.hardware.temperature1);
        runtimePrefs.putString("userId", AppState::current.network.userId);
        runtimePrefs.putString("gsFree", AppState::current.network.gsFree);
        runtimePrefs.putString("realTime", AppState::current.control.realTime);
        runtimePrefs.putString("realDate", AppState::current.control.realDate);
        
        xSemaphoreGive(AppState::mutex);
        Serial.println("Saved non-critical state to NVS (batch)");
    }
    
    // Migration from EEPROM to Preferences
    void migrateCredentialsFromEEPROM() {
        // Check if migration already done
        if (credentialsPrefs.getBool("migrated", false)) {
            Serial.println("Credentials already migrated to Preferences");
            return;
        }
        
        Serial.println("Starting EEPROM to Preferences migration...");
        
        // Read from EEPROM
        String ssid, password, email, userPassword;
        readCredentials(ssid, password, email, userPassword);
        
        if (ssid.length() > 0 && password.length() > 0 && email.length() > 0 && userPassword.length() > 0) {
            // Write to Preferences
            credentialsPrefs.putString("ssid", ssid);
            credentialsPrefs.putString("password", password);
            credentialsPrefs.putString("email", email);
            credentialsPrefs.putString("userPassword", userPassword);
            credentialsPrefs.putBool("migrated", true);
            
            Serial.println("Credentials migrated successfully");
            Serial.println("SSID: " + ssid);
            Serial.println("Email: " + email);
        } else {
            Serial.println("No valid credentials found in EEPROM");
        }
    }
    
    // Auto-save task
    void startAutoSaveTask() {
        xTaskCreate(autoSaveTask, "AutoSave", 2048, NULL, 1, NULL);
    }
    
    void autoSaveTask(void* pvParameters) {
        for (;;) {
            vTaskDelay(30000 / portTICK_PERIOD_MS);  // Check every 30 seconds
            
            if (AppState::stateDirty) {
                saveAllState();
                AppState::clearDirty();
                Serial.println("Auto-saved dirty state");
            } else {
                saveNonCriticalState();  // Save non-critical state periodically
            }
        }
    }
    
    // Credential management
    void saveCredentials(const String &ssid, const String &password, const String &email, const String &userPassword) {
        credentialsPrefs.putString("ssid", ssid);
        credentialsPrefs.putString("password", password);
        credentialsPrefs.putString("email", email);
        credentialsPrefs.putString("userPassword", userPassword);
        Serial.println("Credentials saved to Preferences");
    }
    
    void loadCredentials(String &ssid, String &password, String &email, String &userPassword) {
        ssid = credentialsPrefs.getString("ssid", "");
        password = credentialsPrefs.getString("password", "");
        email = credentialsPrefs.getString("email", "");
        userPassword = credentialsPrefs.getString("userPassword", "");
    }
    
    void eraseCredentials() {
        credentialsPrefs.clear();
        Serial.println("Credentials erased from Preferences");
    }
}
