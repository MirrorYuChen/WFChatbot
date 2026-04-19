/*
 * @Author: chenjingyu
 * @Date: 2024-06-25 17:01:30
 * @Contact: 2458006466@qq.com
 * @Description: AutoTimer
 */
#pragma once

#include "Api.h"
#include <stdint.h>
#include <stdio.h>

NAMESPACE_BEGIN
class API TimerInternal {
public:
  TimerInternal();
  ~TimerInternal();
  NOT_ALLOWED_MOVE(TimerInternal)

  // reset timer
  void reset();
  // get duration (us) from init or latest reset.
  uint64_t durationInUs() const;

  // Get Current Time
  uint64_t current() const { return last_reset_time_; }

protected:
  uint64_t last_reset_time_{0};
};

/** time tracing util. prints duration between init and deinit. */
class API AutoTime : public TimerInternal {
public:
  AutoTime(int line, const char *func);
  ~AutoTime();

  NOT_ALLOWED_MOVE(AutoTime)

private:
  int line_;
  char *name_;
};

NAMESPACE_END

#define AUTO_TIME NAMESPACE::AutoTime ___t(__LINE__, __func__);
