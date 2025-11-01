// Logger.h
// Minimal leveled logger for Arduino environments.

#pragma once

#include <Arduino.h>
#include <stdarg.h>

enum LogLevel {
  LOG_LEVEL_ERROR = 0,
  LOG_LEVEL_WARN = 1,
  LOG_LEVEL_INFO = 2,
  LOG_LEVEL_DEBUG = 3,
};

class Logger {
 public:
  static void setLevel(LogLevel level) { currentLevel_ = level; }

  static void error(const char* fmt, ...) {
    if (currentLevel_ < LOG_LEVEL_ERROR) return;
    va_list args;
    va_start(args, fmt);
    printFormatted("E", fmt, args);
    va_end(args);
  }

  static void warn(const char* fmt, ...) {
    if (currentLevel_ < LOG_LEVEL_WARN) return;
    va_list args;
    va_start(args, fmt);
    printFormatted("W", fmt, args);
    va_end(args);
  }

  static void info(const char* fmt, ...) {
    if (currentLevel_ < LOG_LEVEL_INFO) return;
    va_list args;
    va_start(args, fmt);
    printFormatted("I", fmt, args);
    va_end(args);
  }

  static void debug(const char* fmt, ...) {
    if (currentLevel_ < LOG_LEVEL_DEBUG) return;
    va_list args;
    va_start(args, fmt);
    printFormatted("D", fmt, args);
    va_end(args);
  }

 private:
  static void printPrefix(const char* level) {
    Serial.print('[');
    Serial.print(level);
    Serial.print("] ");
  }

  static void printFormatted(const char* level, const char* fmt, va_list args) {
    printPrefix(level);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    Serial.println(buffer);
  }

  static LogLevel currentLevel_;
};


