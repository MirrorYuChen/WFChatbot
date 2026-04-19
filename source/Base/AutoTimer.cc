/*
 * @Author: chenjingyu
 * @Date: 2024-06-25 17:03:44
 * @Contact: 2458006466@qq.com
 * @Description: AutoTimer
 */
#include "Base/AutoTimer.h"
#include "Base/Logger.h"

#include <cstdlib>
#include <cstring>

#if defined(_MSC_VER)
#define strdup ::_strdup
#else
#include <sys/time.h>
#endif

NAMESPACE_BEGIN
TimerInternal::TimerInternal() {
  reset();
}

TimerInternal::~TimerInternal() = default;

void TimerInternal::reset() {
#if defined(_MSC_VER)
  LARGE_INTEGER time, freq;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&time);
  uint64_t sec = time.QuadPart / freq.QuadPart;
  uint64_t usec = (time.QuadPart % freq.QuadPart) * 1000000 / freq.QuadPart;
  last_reset_time_ = sec * 1000000 + usec;
#else
  struct timeval Current;
  gettimeofday(&Current, nullptr);
  last_reset_time_ = Current.tv_sec * 1000000 + Current.tv_usec;
#endif
}

uint64_t TimerInternal::durationInUs() const {
#if defined(_MSC_VER)
  LARGE_INTEGER time, freq;
  QueryPerformanceCounter(&time);
  QueryPerformanceFrequency(&freq);
  uint64_t sec = time.QuadPart / freq.QuadPart;
  uint64_t u_sec = (time.QuadPart % freq.QuadPart) * 1000000 / freq.QuadPart;
  auto lastTime = sec * 1000000 + u_sec;
#else
  struct timeval Current;
  gettimeofday(&Current, nullptr);
  auto lastTime = Current.tv_sec * 1000000 + Current.tv_usec;
#endif

  return lastTime - last_reset_time_;
}

AutoTime::AutoTime(int line, const char *func) : TimerInternal() {
  name_ = strdup(func);
  line_ = line;
}

AutoTime::~AutoTime() {
  auto timeInUs = durationInUs();
  PrintInfo("%s, %d, cost time: %f ms\n", name_, line_, (float)timeInUs / 1000.0f);
  free(name_);
}

NAMESPACE_END
