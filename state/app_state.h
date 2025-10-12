#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace AppState {
    // State structure - all application variables in one place
    struct State {
        // Hardware configuration
        struct {
            const int geyser1Pin = 15;
            double temperature1 = 0.0;
        } hardware;
        
        // Network status
        struct {
            bool wifiConnected = false;
            bool firebaseReady = false;
            String userId = "";
            String gsFree = "/GeyserSwitch";
        } network;
        
        // Control state
        struct {
            bool geyserOn = false;
            bool timerFlags[6] = {false, false, false, false, false}; // 04:00, 06:00, 08:00, 16:00, 18:00
            bool customTimerFlag = false;
            String realTime = "";
            String realDate = "";
        } control;
        
        // Configuration cache
        struct {
            bool timer04 = false;
            bool timer06 = false;
            bool timer08 = false;
            bool timer16 = false;
            bool timer18 = false;
            String customTimer = "";
            double maxTemp1 = 75.0;
            unsigned long lastRefreshMs = 0;
        } cache;
        
        // System flags
        struct {
            bool firebaseInitialized = false;
            bool userPathInit = false;
            bool wasOnline = false;
            bool postFirebaseInitDone = false;
        } system;
    };
    
    // Global state instance
    extern State current;
    extern SemaphoreHandle_t mutex;
    
    // Initialization
    void init();
    void cleanup();
    
    // Thread-safe accessors for critical state
    inline int getGeyser1Pin() { return current.hardware.geyser1Pin; }
    inline bool isGeyserOn() { 
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool state = current.control.geyserOn;
        xSemaphoreGive(mutex);
        return state;
    }
    
    // Getters for timer flags
    bool getTimerFlag(int index);
    bool getCustomTimerFlag();
    
    // Getters for non-critical state
    double getTemperature1();
    String getUserId();
    String getGsFree();
    bool isWifiConnected();
    bool isFirebaseReady();
    String getRealTime();
    String getRealDate();
    
    // System state getters
    bool isPostFirebaseInitDone();
    void setPostFirebaseInitDone(bool done);
    
    // Critical state (immediate save - must survive power loss)
    void setGeyserOn(bool on);
    void setTimerFlag(int index, bool triggered);
    void setCustomTimerFlag(bool triggered);
    
    // Non-critical setters (batch save every 30s)
    void setTemperature1(double temp);
    void setUserId(const String& id);
    void setGsFree(const String& path);
    void setWifiConnected(bool connected);
    void setFirebaseReady(bool ready);
    void setRealTime(const String& time);
    void setRealDate(const String& date);
    
    // Cache accessors
    void setCacheTimer(int index, bool value);
    void setCacheCustomTimer(const String& timer);
    void setCacheMaxTemp(double temp);
    
    // Dirty flag management
    extern volatile bool stateDirty;
    void markDirty();
    void clearDirty();
}
#endif // APP_STATE_H
