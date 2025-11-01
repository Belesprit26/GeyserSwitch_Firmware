#pragma once

#include <Arduino.h>

// 128-bit UUIDs for GeyserSwitch BLE service and characteristics.
// These are placeholders and can be regenerated if needed.
namespace BleUuids {

// Service
static const char* const SERVICE_GEYSERSWITCH = "8b8a0000-7c9c-4a3f-b3a6-02b8a0f0d101";

// Characteristics
static const char* const CHAR_COMMAND          = "8b8a0001-7c9c-4a3f-b3a6-02b8a0f0d101"; // bool write
static const char* const CHAR_STATE            = "8b8a0002-7c9c-4a3f-b3a6-02b8a0f0d101"; // bool notify/read
static const char* const CHAR_TEMPC            = "8b8a0003-7c9c-4a3f-b3a6-02b8a0f0d101"; // float notify/read
static const char* const CHAR_MAXTEMPC         = "8b8a0004-7c9c-4a3f-b3a6-02b8a0f0d101"; // float read/write
static const char* const CHAR_HYSTERESISC      = "8b8a0005-7c9c-4a3f-b3a6-02b8a0f0d101"; // float read/write
static const char* const CHAR_TIMERS_BITMASK   = "8b8a0006-7c9c-4a3f-b3a6-02b8a0f0d101"; // uint8 read/write
static const char* const CHAR_CUSTOMTIME       = "8b8a0007-7c9c-4a3f-b3a6-02b8a0f0d101"; // 5-char string read/write
static const char* const CHAR_LASTUPDATETIME   = "8b8a0008-7c9c-4a3f-b3a6-02b8a0f0d101"; // string notify/read
static const char* const CHAR_LASTUPDATEDATE   = "8b8a0009-7c9c-4a3f-b3a6-02b8a0f0d101"; // string notify/read
static const char* const CHAR_TIMESYNC_EPOCH   = "8b8a000A-7c9c-4a3f-b3a6-02b8a0f0d101"; // uint32 write
static const char* const CHAR_USAGE_TOTAL_TODAY= "8b8a000B-7c9c-4a3f-b3a6-02b8a0f0d101"; // uint32 read/notify (optional)

// Timers bit positions: 0=04:00, 1=06:00, 2=08:00, 3=16:00, 4=18:00, 5=CUSTOM
inline uint8_t packTimers(bool t0400, bool t0600, bool t0800, bool t1600, bool t1800, bool custom) {
  uint8_t m = 0;
  if (t0400) m |= (1u << 0);
  if (t0600) m |= (1u << 1);
  if (t0800) m |= (1u << 2);
  if (t1600) m |= (1u << 3);
  if (t1800) m |= (1u << 4);
  if (custom) m |= (1u << 5);
  return m;
}

inline void unpackTimers(uint8_t m, bool &t0400, bool &t0600, bool &t0800, bool &t1600, bool &t1800, bool &custom) {
  t0400 = (m & (1u << 0)) != 0;
  t0600 = (m & (1u << 1)) != 0;
  t0800 = (m & (1u << 2)) != 0;
  t1600 = (m & (1u << 3)) != 0;
  t1800 = (m & (1u << 4)) != 0;
  custom = (m & (1u << 5)) != 0;
}

}


