/*
 * @Author: chenjingyu
 * @Date: 2024-09-20 11:06:48
 * @Contact: 2458006466@qq.com
 * @Description: Logger
 */
#include "Base/Logger.h"

#include <cstdlib>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#if OS_ANDROID
#include <spdlog/sinks/android_sink.h>
#endif

#if SPDLOG_VER_MAJOR >= 1
#if defined(__ANDROID__)
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#if defined(_MSC_VER)
#include <spdlog/sinks/stdout_sinks.h>
#endif
#endif
#endif

NAMESPACE_BEGIN
std::shared_ptr<spdlog::logger> CreateDefaultLogger() {
  spdlog::set_level(spdlog::level::trace);
  constexpr const auto logger_name = "spdlog";
#if defined(__ANDROID__)
  return spdlog::android_logger_mt(logger_name);
#elif defined(_MSC_VER)
  return spdlog::stdout_logger_mt(logger_name);
#else
  return spdlog::stdout_color_mt(logger_name);
#endif
}

std::shared_ptr<spdlog::logger> &gLogger() {
  // ! leaky singleton
  static auto ptr = new std::shared_ptr<spdlog::logger>{CreateDefaultLogger()};
  return *ptr;
}

spdlog::logger *getLogger() { return gLogger().get(); }

void setLogger(spdlog::logger *logger) {
  gLogger() = std::shared_ptr<spdlog::logger>(logger, [](auto) {});
}

const char *getShortName(const std::string &path) {
  if (path.empty()) {
    return path.data();
  }
  const size_t pos = path.find_last_of("/\\");
  return path.data() + ((pos == std::string::npos) ? 0 : pos + 1);
}

LogStream::LogStream(const spdlog::source_loc &loc,
                     spdlog::level::level_enum lvl, std::string prefix)
    : loc_(loc), lvl_(lvl), prefix_(std::move(prefix)) {}

LogStream::~LogStream() {
  getLogger()->log(loc_, lvl_, (prefix_ + str()).c_str());
}

LogStreamNone &LogStreamNone::getInstance() {
  static LogStreamNone instance;
  return instance;
}

spdlog::logger *CreateLogger(const std::string &log_file,
                             const std::string &tag) {
  std::vector<spdlog::sink_ptr> sink_vec;
#if defined(__ANDROID__)
  sink_vec.push_back(std::make_shared<spdlog::sinks::android_sink_mt>(tag));
#else
  sink_vec.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#endif
  if (!log_file.empty()) {
    sink_vec.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_file, 1024 * 1024 * 5, 5));
  }
  std::shared_ptr<spdlog::logger> instance = std::make_shared<spdlog::logger>(
      tag.c_str(), sink_vec.begin(), sink_vec.end());
  spdlog::register_logger(instance);
  spdlog::flush_every(std::chrono::seconds(3));
  return instance.get();
}

NAMESPACE_END
