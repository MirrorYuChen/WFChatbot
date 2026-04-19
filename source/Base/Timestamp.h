/*
 * @Author: chenjingyu
 * @Date: 2025-04-16 11:29:00
 * @Contact: 2458006466@qq.com
 * @Description: Timestamp
 */
#pragma once

#include "Api.h"

#include <string>
#include <cstdint>

NAMESPACE_BEGIN
class API Timestamp {
public:
  Timestamp();
  ~Timestamp() = default;

  Timestamp(const Timestamp &other);
  Timestamp &operator=(const Timestamp &other);

  explicit Timestamp(uint64_t micro_sec_since_epoch);

  void Swap(Timestamp &other);

  [[nodiscard]] std::string ToString() const;
  [[nodiscard]] std::string ToFormattedString() const;
  std::string ToFormattedString(const char *fmt) const;

  [[nodiscard]] uint64_t MicroSecondsSinceEpoch() const;

  [[nodiscard]] bool Valid() const { return micro_sec_since_epoch_ > 0; }

  static Timestamp Now();

  static Timestamp Invalid() { return {}; }

  static constexpr const int k_micro_sec_per_sec = 1000 * 1000;

private:
  uint64_t micro_sec_since_epoch_ = 0;
};

inline bool operator<(const Timestamp &lhs, const Timestamp &rhs) {
  return lhs.MicroSecondsSinceEpoch() < rhs.MicroSecondsSinceEpoch();
}

inline bool operator>(const Timestamp &lhs, const Timestamp &rhs) {
  return lhs.MicroSecondsSinceEpoch() > rhs.MicroSecondsSinceEpoch();
}

inline bool operator<=(const Timestamp &lhs, const Timestamp &rhs) {
  return lhs.MicroSecondsSinceEpoch() <= rhs.MicroSecondsSinceEpoch();
}

inline bool operator>=(const Timestamp &lhs, const Timestamp &rhs) {
  return lhs.MicroSecondsSinceEpoch() >= rhs.MicroSecondsSinceEpoch();
}

inline bool operator==(const Timestamp &lhs, const Timestamp &rhs) {
  return lhs.MicroSecondsSinceEpoch() == rhs.MicroSecondsSinceEpoch();
}

inline bool operator!=(const Timestamp &lhs, const Timestamp &rhs) {
  return lhs.MicroSecondsSinceEpoch() != rhs.MicroSecondsSinceEpoch();
}

inline Timestamp operator+(const Timestamp &lhs, uint64_t ms) {
  return Timestamp(lhs.MicroSecondsSinceEpoch() + ms);
}

inline Timestamp operator+(const Timestamp &lhs, double seconds) {
  auto delta = static_cast<uint64_t>(seconds * Timestamp::k_micro_sec_per_sec);
  return Timestamp(lhs.MicroSecondsSinceEpoch() + delta);
}

inline Timestamp operator-(const Timestamp &lhs, uint64_t ms) {
  return Timestamp(lhs.MicroSecondsSinceEpoch() - ms);
}

inline Timestamp operator-(const Timestamp &lhs, double seconds) {
  auto delta = static_cast<uint64_t>(seconds * Timestamp::k_micro_sec_per_sec);
  return Timestamp(lhs.MicroSecondsSinceEpoch() - delta);
}

inline double operator-(const Timestamp &high, const Timestamp &low) {
  auto diff = high.MicroSecondsSinceEpoch() - low.MicroSecondsSinceEpoch();
  return static_cast<double>(diff) / Timestamp::k_micro_sec_per_sec;
}

NAMESPACE_END
