#include <flute/common/Timestamp.h>

#include <stdio.h>
#include <sys/time.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

using namespace flute;

static_assert(sizeof(Timestamp) == sizeof(int64_t),
              "Timestamp is same size as int64_t");

string Timestamp::to_string() const {
  char buf[32] = {0};
  int64_t seconds = m_micro_seconds_since_epoch / kMicroSecondsPerSecond;
  int64_t microseconds = m_micro_seconds_since_epoch % kMicroSecondsPerSecond;
  snprintf(buf, sizeof(buf) - 1, "%" PRId64 ".%06" PRId64 "", seconds,
           microseconds);
  return buf;
}

string Timestamp::to_formatted_string(bool show_micro_seconds) const {
  char buf[64] = {0};
  time_t seconds =
      static_cast<time_t>(m_micro_seconds_since_epoch / kMicroSecondsPerSecond);
  struct tm tm_time;
  gmtime_r(&seconds, &tm_time);

  if (show_micro_seconds) {
    int microseconds =
        static_cast<int>(m_micro_seconds_since_epoch % kMicroSecondsPerSecond);
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, microseconds);
  } else {
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
  }
  return buf;
}

// get current time 
Timestamp Timestamp::now() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  int64_t seconds = tv.tv_sec;
  // second + micro second
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}
