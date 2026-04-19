/*
 * @Author: chenjingyu
 * @Date: 2024-05-27 14:37:45
 * @Contact: 2458006466@qq.com
 * @Description: Logger
 */
#pragma once

// clang-format off
#include "Api.h"
#include <chrono>
#include <memory>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/printf.h>

#define FILENAME NAMESPACE::getShortName(__FILE__)

#define MIRROR_LEVEL_TRACE SPDLOG_LEVEL_TRACE
#define MIRROR_LEVEL_DEBUG SPDLOG_LEVEL_DEBUG
#define MIRROR_LEVEL_INFO SPDLOG_LEVEL_INFO
#define MIRROR_LEVEL_WARN SPDLOG_LEVEL_WARN
#define MIRROR_LEVEL_ERROR SPDLOG_LEVEL_ERROR
#define MIRROR_LEVEL_CRITICAL SPDLOG_LEVEL_CRITICAL
#define MIRROR_LEVEL_OFF SPDLOG_LEVEL_OFF
// clang-format on

#if !defined(MIRROR_ACTIVE_LEVEL)
#define MIRROR_ACTIVE_LEVEL SPDLOG_ACTIVE_LEVEL
#endif

#if MIRROR_ACTIVE_LEVEL <= MIRROR_LEVEL_TRACE
#define FormatTrace(fmt, ...)                                                  \
  NAMESPACE::LogFormat({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::trace, fmt, ##__VA_ARGS__);
#define PrintTrace(fmt, ...)                                                   \
  NAMESPACE::LogPrintf({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::trace, fmt, ##__VA_ARGS__);
#define StreamTrace                                                            \
  NAMESPACE::LogStream({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::trace, "")
#else
#define FormatTrace(...) (void)0;
#define PrintTrace(...) (void)0;
#define StreamTrace NAMESPACE::LogStreamNone::getInstance()
#endif

#if MIRROR_ACTIVE_LEVEL <= MIRROR_LEVEL_DEBUG
#define FormatDebug(fmt, ...)                                                  \
  NAMESPACE::LogFormat({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::debug, fmt, ##__VA_ARGS__);
#define PrintDebug(fmt, ...)                                                   \
  NAMESPACE::LogPrintf({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::debug, fmt, ##__VA_ARGS__);
#define StreamDebug                                                            \
  NAMESPACE::LogStream({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::debug, "")
#else
#define FormatDebug(...) (void)0;
#define PrintDebug(...) (void)0;
#define StreamDebug NAMESPACE::LogStreamNone::getInstance()
#endif

#if MIRROR_ACTIVE_LEVEL <= MIRROR_LEVEL_INFO
#define FormatInfo(fmt, ...)                                                   \
  NAMESPACE::LogFormat({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::info, fmt, ##__VA_ARGS__);
#define PrintInfo(fmt, ...)                                                    \
  NAMESPACE::LogPrintf({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::info, fmt, ##__VA_ARGS__);
#define StreamInfo                                                             \
  NAMESPACE::LogStream({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::info, "")
#else
#define FormatInfo(...) (void)0;
#define PrintInfo(...) (void)0;
#define StreamInfo NAMESPACE::LogStreamNone::getInstance()
#endif

#if MIRROR_ACTIVE_LEVEL <= MIRROR_LEVEL_WARN
#define FormatWarn(fmt, ...)                                                   \
  NAMESPACE::LogFormat({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::warn, fmt, ##__VA_ARGS__);
#define PrintWarn(fmt, ...)                                                    \
  NAMESPACE::LogPrintf({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::warn, fmt, ##__VA_ARGS__);
#define StreamWarn                                                             \
  NAMESPACE::LogStream({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::warn, "")
#else
#define FormatWarn(...) (void)0;
#define PrintWarn(...) (void)0;
#define StreamWarn NAMESPACE::LogStreamNone::getInstance()
#endif

#if MIRROR_ACTIVE_LEVEL <= MIRROR_LEVEL_ERROR
#define FormatError(fmt, ...)                                                  \
  NAMESPACE::LogFormat({FILENAME, __LINE__, __FUNCTION__}, spdlog::level::err, \
                       fmt, ##__VA_ARGS__);
#define PrintError(fmt, ...)                                                   \
  NAMESPACE::LogPrintf({FILENAME, __LINE__, __FUNCTION__}, spdlog::level::err, \
                       fmt, ##__VA_ARGS__);
#define StreamError                                                            \
  NAMESPACE::LogStream({FILENAME, __LINE__, __FUNCTION__}, spdlog::level::err, \
                       "")
#else
#define FormatError(...) (void)0;
#define PrintError(...) (void)0;
#define StreamError NAMESPACE::LogStreamNone::getInstance()
#endif

#if MIRROR_ACTIVE_LEVEL <= MIRROR_LEVEL_CRITICAL
#define FormatCritical(fmt, ...)                                               \
  NAMESPACE::LogFormat({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::critical, fmt, ##__VA_ARGS__);
#define PrintCritical(fmt, ...)                                                \
  NAMESPACE::LogPrintf({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::critical, fmt, ##__VA_ARGS__);
#define StreamCritical                                                         \
  NAMESPACE::LogStream({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::critical, "")
#else
#define FormatCritical(...) (void)0;
#define PrintCritical(...) (void)0;
#define StreamCritical NAMESPACE::LogStreamNone::getInstance()
#endif

// clang-format on
#define CHECK(x)                                                               \
  if (!(x))                                                                    \
  NAMESPACE::LogStream({FILENAME, __LINE__, __FUNCTION__},                     \
                       spdlog::level::critical, "Check failed: ")              \
      << #x << ": " // NOLINT(*)

#define CHECK_BINARY_IMPL(x, cmp, y) CHECK(x cmp y) << x << #cmp << y << " "
#define CHECK_EQ(x, y) CHECK_BINARY_IMPL(x, ==, y)
#define CHECK_NE(x, y) CHECK_BINARY_IMPL(x, !=, y)
#define CHECK_LT(x, y) CHECK_BINARY_IMPL(x, <, y)
#define CHECK_LE(x, y) CHECK_BINARY_IMPL(x, <=, y)
#define CHECK_GT(x, y) CHECK_BINARY_IMPL(x, >, y)
#define CHECK_GE(x, y) CHECK_BINARY_IMPL(x, >=, y)

NAMESPACE_BEGIN
API spdlog::logger *getLogger();
API void setLogger(spdlog::logger *logger);

template <typename... Args>
void API LogPrintf(const spdlog::source_loc &loc, spdlog::level::level_enum lvl,
                   const char *fmt, Args &&... args) {
  getLogger()->log(loc, lvl, fmt::sprintf(fmt, std::forward<Args>(args)...));
}

inline void API LogPrintf(const spdlog::source_loc &loc, spdlog::level::level_enum lvl,
                          const std::string &str) {
  getLogger()->log(loc, lvl, "{}", str);
}

template <typename... Args>
void API LogFormat(const spdlog::source_loc &loc, spdlog::level::level_enum lvl,
                   const char *fmt, Args &&... args) {
  getLogger()->log(loc, lvl, fmt, std::forward<Args>(args)...);
}

inline void API LogFormat(const spdlog::source_loc &loc, spdlog::level::level_enum lvl,
                          const std::string &str) {
  getLogger()->log(loc, lvl, "{}", str);
}


class API LogStream : public std::ostringstream {
public:
  LogStream(const spdlog::source_loc &loc, spdlog::level::level_enum lvl,
            std::string prefix);

  ~LogStream() final;

private:
  spdlog::source_loc loc_;
  spdlog::level::level_enum lvl_;
  std::string prefix_;
};

API const char *getShortName(const std::string &path);

class API LogStreamNone : public std::ostringstream {
public:
  LogStreamNone() = default;
  ~LogStreamNone() final = default;

  static LogStreamNone &getInstance();
};

API spdlog::logger *CreateLogger(const std::string &log_file,
                                 const std::string &tag);
NAMESPACE_END
