#include <errno.h>
#include <flute/common/LogLine.h>
#include <flute/common/Timestamp.h>
#include <flute/common/Timezone.h>
#include <stdio.h>
#include <string.h>

#include <sstream>

namespace flute {

LogLine::LogLevel LogLine::g_log_level = INFO;

// cached time info

__thread char t_errnobuf[512];
__thread char t_time[64];
__thread time_t t_last_second;

const char* strerror_tl(int saved_errorno) {
  return strerror_r(saved_errorno, t_errnobuf, sizeof(t_errnobuf));
}

LogLine::OutputFuncPtr LogLine::g_output = LogLine::default_output;
LogLine::FlushFuncPtr LogLine::g_flush = LogLine::default_flush;
// UTC+8 Beijing
Timezone LogLine::g_log_timezone(8 * 3600, "CST");

// Implementations of the inner Data class

LogLine::Data::Data(LogLevel level, int old_errno, const BaseName& file,
                    int line, const char* type)
    : m_level{level},
      m_line{line},
      m_basename(file),
      m_log_stream(),
      m_timestamp(Timestamp::now()) {
  if (type != NULL) {
    m_log_stream << type;
  }
  log_time();
}

LogLine::LogLine(BaseName file, int line, const char* type)
    : m_data(INFO, 0, file, line, type) {}
LogLine::LogLine(BaseName file, int line, LogLevel level, const char* type)
    : m_data(level, 0, file, line, type) {}
LogLine::LogLine(BaseName file, int line, LogLevel level, const char* func,
                 const char* type)
    : m_data(level, 0, file, line, type) {
  m_data.m_log_stream << func << ": ";
}
LogLine::LogLine(BaseName file, int line, bool to_abort, const char* type)
    : m_data(to_abort ? FATAL : ERROR, errno, file, line, type) {}

// Additional overload preventing circular reference of LogStream to BaseName
// https://stackoverflow.com/questions/4421706/what-are-the-basic-rules-and-idioms-for-operator-overloading
inline LogStream& operator<<(LogStream& s, const BaseName& v) {
  s.append(v.data(), v.size());
  return s;
}

LogLine::~LogLine() {
  if (errno != 0) {
    m_data.m_log_stream << " [ERRNO:" << errno << "]" << strerror_tl(errno)
                        << " ";
  }
  m_data.m_log_stream << " - " << m_data.m_basename << ':' << m_data.m_line
                      << '\n';
  const LogBuffer& buf(stream().m_buffer);
  g_output(buf.data(), buf.length());
  if (m_data.m_level == FATAL) {
    g_flush();
    abort();
  }
}

void LogLine::Data::log_time() {
  int64_t micro_seconds_since_epoch = m_timestamp.micro_seconds_since_epoch();
  time_t seconds = static_cast<time_t>(micro_seconds_since_epoch /
                                       Timestamp::kMicroSecondsPerSecond);
  int microseconds = static_cast<int>(micro_seconds_since_epoch %
                                      Timestamp::kMicroSecondsPerSecond);
  // time cache failed
  if (seconds != t_last_second) {
    t_last_second = seconds;
    struct tm tm_time;
    if (g_log_timezone.valid()) {
      tm_time = g_log_timezone.to_local_time(seconds);
    } else {
      ::gmtime_r(&seconds, &tm_time);  // FIXME Timezone::fromUtcTime
    }

    int len =
        snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    assert(len == 17);
    (void)len;
  }

  // log time
  if (g_log_timezone.valid()) {
    // micro seconds
    FormattedString us(".%06d ", microseconds);
    assert(us.length_0() == 8);
    m_log_stream << StringPiece(t_time, 17) << StringPiece(us.data(), 8);
  } else {
    FormattedString us(".%06dZ ", microseconds);
    assert(us.length_0() == 9);
    m_log_stream << StringPiece(t_time, 17) << StringPiece(us.data(), 9);
  }
}

void LogLine::set_log_level(const LogLevel& level) { g_log_level = level; }

void LogLine::set_output(const OutputFuncPtr& out) { g_output = out; }

void LogLine::set_flush(const FlushFuncPtr& flush) { g_flush = flush; }

void LogLine::set_timezone(const Timezone& tz) { g_log_timezone = tz; }

}  // namespace flute
