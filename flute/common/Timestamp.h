#ifndef FLUTE_COMMON_TIMESTAMP_H
#define FLUTE_COMMON_TIMESTAMP_H

#include <flute/common/types.h>

namespace flute {

class Timestamp {
 public:
  Timestamp() : m_micro_seconds_since_epoch(0) {}

  explicit Timestamp(int64_t micro_seconds_since_epoch)
      : m_micro_seconds_since_epoch(micro_seconds_since_epoch) {}

  void swap(Timestamp& another) {
    std::swap(m_micro_seconds_since_epoch, another.m_micro_seconds_since_epoch);
  }

  // default copy/assignment/dtor are Okay

  std::string to_string() const;
  std::string to_formatted_string(bool showMicroseconds = true) const;

  bool valid() const { return m_micro_seconds_since_epoch > 0; }

  // for internal usage.
  int64_t micro_seconds_since_epoch() const {
    return m_micro_seconds_since_epoch;
  }

  time_t seconds_since_epoch() const {
    return static_cast<time_t>(m_micro_seconds_since_epoch /
                               kMicroSecondsPerSecond);
  }

  static Timestamp now();
  static Timestamp invalid() { return Timestamp(); }

  static Timestamp from_unix_time(time_t t) { return from_unix_time(t, 0); }

  static Timestamp from_unix_time(time_t t, int microseconds) {
    return Timestamp(static_cast<int64_t>(t) * kMicroSecondsPerSecond +
                     microseconds);
  }

  static const int kMicroSecondsPerSecond = 1000 * 1000;

 private:
  int64_t m_micro_seconds_since_epoch;
};

inline bool operator<(Timestamp lhs, Timestamp rhs) {
  return lhs.micro_seconds_since_epoch() < rhs.micro_seconds_since_epoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs) {
  return lhs.micro_seconds_since_epoch() == rhs.micro_seconds_since_epoch();
}

inline double second_difference(Timestamp high, Timestamp low) {
  int64_t diff =
      high.micro_seconds_since_epoch() - low.micro_seconds_since_epoch();
  return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

inline Timestamp add_second(Timestamp timestamp, double seconds) {
  int64_t delta =
      static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
  return Timestamp(timestamp.micro_seconds_since_epoch() + delta);
}

}  // namespace flute

#endif  // FLUTE_COMMON_TIMESTAMP_H