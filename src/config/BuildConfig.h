// BuildConfig.h
// Central build-time configuration and feature flags.

#pragma once

#define BUILD_LOG_BAUD_RATE 115200

// Task stack sizes and priorities will be defined once FreeRTOS tasks are added.

// Compile-time feature toggles (choose one flavor per build)
// Online flavor: BUILD_ENABLE_RTDB=1, BUILD_ENABLE_BLE=0, USE_MOBIZT_FIREBASE=1
// Offline flavor: BUILD_ENABLE_RTDB=0, BUILD_ENABLE_BLE=1, USE_MOBIZT_FIREBASE=0
// Defaults below keep online flavor; adjust per build.
#ifndef BUILD_ENABLE_RTDB
#define BUILD_ENABLE_RTDB 0
#endif
#ifndef BUILD_ENABLE_BLE
#define BUILD_ENABLE_BLE 1
#endif
#ifndef USE_MOBIZT_FIREBASE
#define USE_MOBIZT_FIREBASE BUILD_ENABLE_RTDB
#endif

// Verbose settings logging (timers and target temperature) every cycle
#ifndef BUILD_LOG_SETTINGS_VERBOSE
#define BUILD_LOG_SETTINGS_VERBOSE 1
#endif

// Remove runtime mode selection; flavor chosen at compile time


