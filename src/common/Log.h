#pragma once

/**
 * ARCANEE - Modern Fantasy Console
 * Copyright (C) 2025 Michele Fabbri
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

#include <chrono>
#include <cstdarg>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace arcanee {

// Log levels per Chapter 11
enum class LogLevel { Trace, Debug, Info, Warning, Error, Fatal };

class Log {
public:
  static void setLevel(LogLevel level) { s_minLevel = level; }
  static LogLevel getLevel() { return s_minLevel; }

  static void trace(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Trace, fmt, args);
    va_end(args);
  }

  static void debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Debug, fmt, args);
    va_end(args);
  }

  static void info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Info, fmt, args);
    va_end(args);
  }

  static void warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Warning, fmt, args);
    va_end(args);
  }

  static void error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Error, fmt, args);
    va_end(args);
  }

  static void fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logv(LogLevel::Fatal, fmt, args);
    va_end(args);
  }

private:
  static void logv(LogLevel level, const char *fmt, va_list args) {
    if (level < s_minLevel)
      return;

    std::lock_guard<std::mutex> lock(s_mutex);

    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    char timeStr[32];
    std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&time));

    // Format message
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    // Level string
    const char *levelStr = "";
    switch (level) {
    case LogLevel::Trace:
      levelStr = "TRACE";
      break;
    case LogLevel::Debug:
      levelStr = "DEBUG";
      break;
    case LogLevel::Info:
      levelStr = "INFO";
      break;
    case LogLevel::Warning:
      levelStr = "WARN";
      break;
    case LogLevel::Error:
      levelStr = "ERROR";
      break;
    case LogLevel::Fatal:
      levelStr = "FATAL";
      break;
    }

    // Output
    std::ostream &out = (level >= LogLevel::Warning) ? std::cerr : std::cout;
    out << "[" << timeStr << "." << std::setw(3) << std::setfill('0')
        << ms.count() << "] [" << levelStr << "] " << buffer << std::endl;
  }

  inline static LogLevel s_minLevel = LogLevel::Info;
  inline static std::mutex s_mutex;
};

// Convenience macros
#define LOG_TRACE(...) arcanee::Log::trace(__VA_ARGS__)
#define LOG_DEBUG(...) arcanee::Log::debug(__VA_ARGS__)
#define LOG_INFO(...) arcanee::Log::info(__VA_ARGS__)
#define LOG_WARN(...) arcanee::Log::warn(__VA_ARGS__)
#define LOG_ERROR(...) arcanee::Log::error(__VA_ARGS__)
#define LOG_FATAL(...) arcanee::Log::fatal(__VA_ARGS__)

} // namespace arcanee
