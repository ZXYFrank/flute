

#ifndef FLUTE_COMMON_TIMEZONE_H
#define FLUTE_COMMON_TIMEZONE_H

#include <memory>
#include <time.h>

namespace flute {

// Timezone for 1970~2030
class Timezone {
 public:
  explicit Timezone(const char* zonefile);
  Timezone(int east_of_utc, const char* tzname);  // a fixed timezone
  Timezone() = default;                           // an invalid timezone

  // default copy ctor/assignment/dtor are Okay.

  bool valid() const {
    // 'explicit operator bool() const' in C++11
    return static_cast<bool>(m_data);
  }

  struct tm to_local_time(time_t seconds_since_epoch) const;
  time_t from_local_time(const struct tm&) const;

  // gmtime(3)
  static struct tm to_utc_time(time_t seconds_since_epoch, bool yday = false);
  // timegm(3)
  static time_t from_utc_time(const struct tm&);
  // year in [1900..2500], month in [1..12], day in [1..31]
  static time_t from_utc_time(int year, int month, int day, int hour,
                              int minute, int seconds);

  struct TimeData;

 private:
  std::shared_ptr<TimeData> m_data;
};

}  // namespace flute

#endif  // FLUTE_COMMON_TIMEZONE_H
