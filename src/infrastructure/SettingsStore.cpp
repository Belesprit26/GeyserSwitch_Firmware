// SettingsStore.cpp

#include "SettingsStore.h"
// Implementation notes
// - Uses Arduino Preferences (ESP32 NVS) under the namespace `geyserswitch`.
// - Each field is stored as its own key; timers are stored as individual boolean keys to
//   avoid bringing a JSON dependency and to make partial updates cheap.
#include <Preferences.h>
// No external JSON dependency; store timer flags individually in NVS

// DTO defined in header

static const char* kNs = "geyserswitch";
static const uint16_t kSchemaV = 1;

static void writeStringPref(Preferences &p, const char* key, const String &val) {
  p.putString(key, val);
}

static String readStringPref(Preferences &p, const char* key, const String &defVal = String()) {
  return p.getString(key, defVal);
}

uint32_t SettingsStore::computeCrc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j) {
      uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

// Load settings and last record. If no valid record is present, leave outputs unchanged
// so the caller's defaults remain in effect.
bool SettingsStore::load(SettingsDto &settingsOut, PersistLastRecord &lastOut) {
  Preferences pref;
  if (!pref.begin(kNs, true)) return false;
  bool ok = false;
  uint16_t v = pref.getUShort("cfg_v", 0);
  if (v == kSchemaV) {
    // Load settings scalars/strings
    settingsOut.maxTempC = pref.getFloat("max_temp", settingsOut.maxTempC);
    settingsOut.customTime = readStringPref(pref, "custom", settingsOut.customTime);
    // Load individual timer flags
    settingsOut.t0400 = pref.getBool("t0400", settingsOut.t0400);
    settingsOut.t0600 = pref.getBool("t0600", settingsOut.t0600);
    settingsOut.t0800 = pref.getBool("t0800", settingsOut.t0800);
    settingsOut.t1600 = pref.getBool("t1600", settingsOut.t1600);
    settingsOut.t1800 = pref.getBool("t1800", settingsOut.t1800);

    // Last records
    lastOut.hasLastState = pref.getBool("has_ls", false);
    lastOut.lastStateOn = pref.getBool("ls_on", false);
    lastOut.lastStateTs = pref.getULong("ls_ts", 0);
    lastOut.hasLastCommand = pref.getBool("has_lc", false);
    lastOut.lastCommandOn = pref.getBool("lc_on", false);
    lastOut.lastCommandTs = pref.getULong("lc_ts", 0);
    ok = true;
  }
  pref.end();
  return ok;
}

// Save settings and last record. Caller should debounce/rate‑limit this call.
bool SettingsStore::save(const SettingsDto &settingsIn, const PersistLastRecord &lastIn) {
  Preferences pref;
  if (!pref.begin(kNs, false)) return false;

  pref.putUShort("cfg_v", kSchemaV);
  pref.putFloat("max_temp", settingsIn.maxTempC);
  writeStringPref(pref, "custom", settingsIn.customTime);
  // Store individual timer flags
  pref.putBool("t0400", settingsIn.t0400);
  pref.putBool("t0600", settingsIn.t0600);
  pref.putBool("t0800", settingsIn.t0800);
  pref.putBool("t1600", settingsIn.t1600);
  pref.putBool("t1800", settingsIn.t1800);

  pref.putBool("has_ls", lastIn.hasLastState);
  pref.putBool("ls_on", lastIn.lastStateOn);
  pref.putULong("ls_ts", lastIn.lastStateTs);
  pref.putBool("has_lc", lastIn.hasLastCommand);
  pref.putBool("lc_on", lastIn.lastCommandOn);
  pref.putULong("lc_ts", lastIn.lastCommandTs);

  pref.end();
  return true;
}


