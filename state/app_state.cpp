#include "app_state.h"

namespace AppState {
    State current;
    SemaphoreHandle_t mutex = NULL;
    volatile bool stateDirty = false;
    
    void init() {
        mutex = xSemaphoreCreateMutex();
        if (mutex == NULL) {
            Serial.println("ERROR: Failed to create state mutex!");
            ESP.restart();
        }
        Serial.println("State management initialized");
    }
    
    void cleanup() {
        if (mutex != NULL) {
            vSemaphoreDelete(mutex);
            mutex = NULL;
        }
    }
    
    // Critical state (immediate save - must survive power loss)
    void setGeyserOn(bool on) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        if (current.control.geyserOn != on) {
            current.control.geyserOn = on;
            markDirty();
        }
        xSemaphoreGive(mutex);
    }
    
    void setTimerFlag(int index, bool triggered) {
        if (index < 0 || index >= 5) return;
        xSemaphoreTake(mutex, portMAX_DELAY);
        if (current.control.timerFlags[index] != triggered) {
            current.control.timerFlags[index] = triggered;
            markDirty();
        }
        xSemaphoreGive(mutex);
    }
    
    void setCustomTimerFlag(bool triggered) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        if (current.control.customTimerFlag != triggered) {
            current.control.customTimerFlag = triggered;
            markDirty();
        }
        xSemaphoreGive(mutex);
    }
    
    // Non-critical setters (batch save every 30s)
    void setTemperature1(double temp) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.hardware.temperature1 = temp;
        xSemaphoreGive(mutex);
        // Temperature is non-critical, will be saved in batch
    }
    
    void setUserId(const String& id) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.network.userId = id;
        xSemaphoreGive(mutex);
        // User ID is non-critical, will be saved in batch
    }
    
    void setGsFree(const String& path) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.network.gsFree = path;
        xSemaphoreGive(mutex);
        // Path is non-critical, will be saved in batch
    }
    
    void setWifiConnected(bool connected) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.network.wifiConnected = connected;
        xSemaphoreGive(mutex);
        // WiFi status is non-critical, will be saved in batch
    }
    
    void setFirebaseReady(bool ready) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.network.firebaseReady = ready;
        xSemaphoreGive(mutex);
        // Firebase status is non-critical, will be saved in batch
    }
    
    void setRealTime(const String& time) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.control.realTime = time;
        xSemaphoreGive(mutex);
        // Time is non-critical, will be saved in batch
    }
    
    void setRealDate(const String& date) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.control.realDate = date;
        xSemaphoreGive(mutex);
        // Date is non-critical, will be saved in batch
    }
    
    // Getters for timer flags
    bool getTimerFlag(int index) {
        if (index < 0 || index >= 5) return false;
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool state = current.control.timerFlags[index];
        xSemaphoreGive(mutex);
        return state;
    }
    
    bool getCustomTimerFlag() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool state = current.control.customTimerFlag;
        xSemaphoreGive(mutex);
        return state;
    }
    
    // Getters for non-critical state
    double getTemperature1() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        double temp = current.hardware.temperature1;
        xSemaphoreGive(mutex);
        return temp;
    }
    
    String getUserId() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        String id = current.network.userId;
        xSemaphoreGive(mutex);
        return id;
    }
    
    String getGsFree() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        String path = current.network.gsFree;
        xSemaphoreGive(mutex);
        return path;
    }
    
    bool isWifiConnected() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool connected = current.network.wifiConnected;
        xSemaphoreGive(mutex);
        return connected;
    }
    
    bool isFirebaseReady() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool ready = current.network.firebaseReady;
        xSemaphoreGive(mutex);
        return ready;
    }
    
    String getRealTime() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        String time = current.control.realTime;
        xSemaphoreGive(mutex);
        return time;
    }
    
    String getRealDate() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        String date = current.control.realDate;
        xSemaphoreGive(mutex);
        return date;
    }
    
    // System state getters
    bool isPostFirebaseInitDone() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool done = current.system.postFirebaseInitDone;
        xSemaphoreGive(mutex);
        return done;
    }
    
    void setPostFirebaseInitDone(bool done) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.system.postFirebaseInitDone = done;
        xSemaphoreGive(mutex);
    }
    
    // Cache setters
    void setCacheTimer(int index, bool value) {
        if (index < 0 || index >= 5) return;
        xSemaphoreTake(mutex, portMAX_DELAY);
        switch (index) {
            case 0: current.cache.timer04 = value; break;
            case 1: current.cache.timer06 = value; break;
            case 2: current.cache.timer08 = value; break;
            case 3: current.cache.timer16 = value; break;
            case 4: current.cache.timer18 = value; break;
        }
        xSemaphoreGive(mutex);
    }
    
    void setCacheCustomTimer(const String& timer) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.cache.customTimer = timer;
        xSemaphoreGive(mutex);
    }
    
    void setCacheMaxTemp(double temp) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        current.cache.maxTemp1 = temp;
        xSemaphoreGive(mutex);
    }
    
    void markDirty() {
        stateDirty = true;
    }
    
    void clearDirty() {
        stateDirty = false;
    }
}
