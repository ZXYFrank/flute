#ifndef FLUTE_COMMON_LOGLINE_H
#define FLUTE_COMMON_LOGLINE_H

#include <flute/common/LogStream.h>
#include <flute/common/Timestamp.h>
#include <flute/common/Timezone.h>

namespace flute {

extern __thread char t_errnobuf[512];
extern __thread char t_time[64];
extern __thread time_t t_last_second;
const char* strerror_tl(int savedErrno);

// compile time calculation of basename of source file
class BaseName {
 public:
  template <int N>
  BaseName(const char (&arr)[N]) : m_data(arr), m_size(N - 1) {
    const char* slash = strrchr(m_data, '/');  // builtin function
    if (slash) {
      m_data = slash + 1;
      m_size -= static_cast<int>(m_data - arr);
    }
  }

  explicit BaseName(const char* filename) : m_data(filename) {
    const char* slash = strrchr(filename, '/');
    if (slash) {
      m_data = slash + 1;
    }
    m_size = static_cast<int>(strlen(m_data));
  }

  inline const char* data() const { return m_data; }
  inline int size() const { return m_size; }

 private:
  const char* m_data;
  int m_size;
};

class LogLine {
 public:
  enum LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    NUM_LOG_LEVELS,
  };

  static LogLevel g_log_level;

 public:
  LogLine(BaseName file, int line, const char* name = NULL);
  LogLine(BaseName file, int line, LogLevel level, const char* name = NULL);
  LogLine(BaseName file, int line, LogLevel level, const char* func,
          const char* name);
  LogLine(BaseName file, int line, bool to_abort, const char* name);

  // output function pointer
  typedef void (*OutputFuncPtr)(const char* msg, int len);
  typedef void (*FlushFuncPtr)();

  // Global Log settings
  static LogLevel get_log_level() { return g_log_level; }
  static void set_log_level(const LogLevel& level);
  static void set_output(const OutputFuncPtr& out);
  static void set_flush(const FlushFuncPtr& flush);
  static void set_timezone(const Timezone& tz);
  LogStream& stream() { return m_data.m_log_stream; }

  static void default_output(const char* msg, int len) {
    size_t n_bytes = fwrite(msg, 1, len, stdout);
    assert(n_bytes > 0);
  }
  static void default_flush() { fflush(stdout); }

  // TODO: Timezone

  ~LogLine();

  static OutputFuncPtr g_output;
  static FlushFuncPtr g_flush;
  static Timezone g_log_timezone;

 private:
  class Data {
   public:
    explicit Data(LogLevel level, int old_errno, const BaseName& file, int line,
                  const char* type);

   public:
    Timestamp m_timestamp;
    LogStream m_log_stream;
    BaseName m_basename;
    LogLevel m_level;
    int m_line;
    void log_time();
  };

 private:
  Data m_data;
};

static const char* const LogLevelNames[LogLine::NUM_LOG_LEVELS] = {
    "TRACE ", "DEBUG ", "INFO  ", "WARN  ", "ERROR ", "FATAL ",
};

// A small helper for CHECK_NOTNULL().
template <typename T>
T* CheckNotNull(BaseName file, int line, const char* names, T* ptr) {
  if (ptr == NULL) {
    LogLine(file, line, LogLine::FATAL, "[FATAL] ").stream() << names;
  }
  return ptr;
}

}  // namespace flute

// Macros for Logging

#ifndef LOG_TYPE_STRING
#define LOG_TYPE_STRING flute::LogLevelNames[flute::LogLine::get_log_level()]
#endif

#define LOG_TRACE                                                     \
  if (flute::LogLine::get_log_level() <= flute::LogLine::TRACE)       \
  flute::LogLine(__FILE__, __LINE__, flute::LogLine::TRACE, __func__, \
                 "[TRACE] ")                                          \
      .stream()
#define LOG_DEBUG                                                     \
  if (flute::LogLine::get_log_level() <= flute::LogLine::DEBUG)       \
  flute::LogLine(__FILE__, __LINE__, flute::LogLine::DEBUG, __func__, \
                 "[DEBUG] ")                                          \
      .stream()
#define LOG_INFO                                               \
  if (flute::LogLine::get_log_level() <= flute::LogLine::INFO) \
  flute::LogLine(__FILE__, __LINE__, "[INFO]  ").stream()
#define LOG_WARN \
  flute::LogLine(__FILE__, __LINE__, flute::LogLine::WARN, "[WARN]  ").stream()
#define LOG_ERROR \
  flute::LogLine(__FILE__, __LINE__, flute::LogLine::ERROR, "[ERROR] ").stream()
#define LOG_IF_ERROR \
  if (errno != 0)    \
  flute::LogLine(__FILE__, __LINE__, flute::LogLine::ERROR, "[ERROR] ").stream()
#define LOG_FATAL \
  flute::LogLine(__FILE__, __LINE__, flute::LogLine::FATAL, "[FATAL] ").stream()
#define LOG_SYSERR \
  flute::LogLine(__FILE__, __LINE__, false, "[SERROR]").stream()
#define LOG_SYSFATAL \
  flute::LogLine(__FILE__, __LINE__, true, "[SFATAL]").stream()

// Taken from glog/logging.h
//
// Check that the input is non NULL.  This very useful in constructor
// initializer lists.

#define CHECK_NOTNULL(val) \
  flute::CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

#endif  // FLUTE_COMMON_LOGLINE_H