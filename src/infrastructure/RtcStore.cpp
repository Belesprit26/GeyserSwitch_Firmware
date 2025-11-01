// RtcStore.cpp

#include "RtcStore.h"

#if defined(ESP32)
#include <esp_system.h>
#include <string.h>

// RTC slow memory layout (keep small and aligned)
struct __attribute__((packed)) RtcBlock {
  uint32_t magic;
  uint32_t version;
  uint8_t hasLastState;
  uint8_t lastStateOn;
  uint32_t lastStateTs;
  uint8_t hasLastCommand;
  uint8_t lastCommandOn;
  uint32_t lastCommandTs;
  uint32_t crc;
};

static constexpr uint32_t kMagic = 0x47535731; // GSW1
static constexpr uint32_t kVersion = 1;

static RTC_DATA_ATTR RtcBlock rtcBlock;

static uint32_t crc32(const uint8_t* data, size_t len) {
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

bool RtcStore::load(PersistLastRecord &out) {
  if (rtcBlock.magic != kMagic || rtcBlock.version != kVersion) return false;
  uint32_t calc = crc32(reinterpret_cast<const uint8_t*>(&rtcBlock), sizeof(RtcBlock) - sizeof(uint32_t));
  if (calc != rtcBlock.crc) return false;
  out.hasLastState = rtcBlock.hasLastState;
  out.lastStateOn = rtcBlock.lastStateOn;
  out.lastStateTs = rtcBlock.lastStateTs;
  out.hasLastCommand = rtcBlock.hasLastCommand;
  out.lastCommandOn = rtcBlock.lastCommandOn;
  out.lastCommandTs = rtcBlock.lastCommandTs;
  return true;
}

void RtcStore::save(const PersistLastRecord &in) {
  rtcBlock.magic = kMagic;
  rtcBlock.version = kVersion;
  rtcBlock.hasLastState = in.hasLastState;
  rtcBlock.lastStateOn = in.lastStateOn;
  rtcBlock.lastStateTs = in.lastStateTs;
  rtcBlock.hasLastCommand = in.hasLastCommand;
  rtcBlock.lastCommandOn = in.lastCommandOn;
  rtcBlock.lastCommandTs = in.lastCommandTs;
  rtcBlock.crc = crc32(reinterpret_cast<const uint8_t*>(&rtcBlock), sizeof(RtcBlock) - sizeof(uint32_t));
}

void RtcStore::clear() {
  memset(&rtcBlock, 0, sizeof(rtcBlock));
}

#else

bool RtcStore::load(PersistLastRecord &) { return false; }
void RtcStore::save(const PersistLastRecord &) {}
void RtcStore::clear() {}

#endif


