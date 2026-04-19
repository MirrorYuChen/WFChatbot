/*
 * @Author: chenjingyu
 * @Date: 2025-04-16 11:29:07
 * @Contact: 2458006466@qq.com
 * @Description: Timestamp
 */
#include "Base/Timestamp.h"
#include <chrono>
#include <iomanip> // put_time
#include <sstream>

using namespace std::chrono;

NAMESPACE_BEGIN
static_assert(sizeof(Timestamp) == sizeof(uint64_t), "Timestamp should be same size as uint64_t");
Timestamp::Timestamp() : micro_sec_since_epoch_(0) {
}

Timestamp::Timestamp(uint64_t micro_sec_since_epoch)
  : micro_sec_since_epoch_(micro_sec_since_epoch) {
}

Timestamp::Timestamp(const Timestamp &other)
  : micro_sec_since_epoch_(other.micro_sec_since_epoch_) {
}

Timestamp &Timestamp::operator=(const Timestamp &other) {
  micro_sec_since_epoch_ = other.micro_sec_since_epoch_;
  return *this;
}

void Timestamp::Swap(Timestamp &other) {
  std::swap(micro_sec_since_epoch_, other.micro_sec_since_epoch_);
}

std::string Timestamp::ToString() const {
  return std::to_string(micro_sec_since_epoch_ / k_micro_sec_per_sec) + "." +
         std::to_string(micro_sec_since_epoch_ % k_micro_sec_per_sec);
}

std::string Timestamp::ToFormattedString() const {
  return ToFormattedString("%Y-%m-%d %X");
}

std::string Timestamp::ToFormattedString(const char *fmt) const {
  std::time_t time = micro_sec_since_epoch_ / k_micro_sec_per_sec; // ms --> s
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), fmt);
  return ss.str();
}

uint64_t Timestamp::MicroSecondsSinceEpoch() const {
  return micro_sec_since_epoch_;
}

Timestamp Timestamp::Now() {
  uint64_t timestamp =
      duration_cast<microseconds>(system_clock::now().time_since_epoch())
          .count();
  return Timestamp(timestamp);
}

NAMESPACE_END
