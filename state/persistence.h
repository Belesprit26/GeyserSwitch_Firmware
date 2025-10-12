#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <Arduino.h>
#include <Preferences.h>
#include "app_state.h"

namespace Persistence {
    // Initialize persistence system
    void init();
    
    // Critical state (immediate save - must survive power loss)
    void saveGeyserState(bool on);
    void saveTimerFlags(bool flags[5]);
    void saveCustomTimerFlag(bool triggered);
    
    // Load critical state
    bool loadGeyserState();
    void loadTimerFlags(bool flags[5]);
    bool loadCustomTimerFlag();
    
    // Batch operations
    void saveAllState();
    void loadAllState();
    void saveNonCriticalState();
    
    // Migration from EEPROM to Preferences
    void migrateCredentialsFromEEPROM();
    
    // Credential management
    void saveCredentials(const String &ssid, const String &password, const String &email, const String &userPassword);
    void loadCredentials(String &ssid, String &password, String &email, String &userPassword);
    void eraseCredentials();
    
    // Auto-save task
    void startAutoSaveTask();
    void autoSaveTask(void* pvParameters);
}
#endif // PERSISTENCE_H
