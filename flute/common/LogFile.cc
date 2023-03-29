#include <assert.h>
#include <flute/common/FileUtil.h>
#include <flute/common/LogFile.h>
#include <flute/common/ProcessInfo.h>
#include <stdio.h>
#include <time.h>

namespace flute {

LogFile::LogFile(const string& basename, off_t rollSize, bool threadSafe,
                 int flushInterval, int checkEveryN)
    : m_basename(basename),
      m_rollsize(rollSize),
      m_flush_interval(flushInterval),
      m_check_every_n_appends(checkEveryN),
      m_count(0),
      m_mutex(threadSafe ? new MutexLock : NULL),
      m_start_time(0),
      m_last_roll(0),
      m_last_flush(0) {
  assert(basename.find('/') == string::npos);
  roll_file();
}

LogFile::~LogFile() = default;

void LogFile::append(const char* logline, int len) {
  if (m_mutex) {
    MutexLockGuard lock(*m_mutex);
    append_unlocked(logline, len);
  } else {
    append_unlocked(logline, len);
  }
}

void LogFile::flush() {
  if (m_mutex) {
    MutexLockGuard lock(*m_mutex);
    m_file->flush();
  } else {
    m_file->flush();
  }
}

void LogFile::append_unlocked(const char* logline, int len) {
  m_file->append(logline, len);

  if (m_file->written_bytes() > m_rollsize) {
    roll_file();
  } else {
    ++m_count;
    if (m_count >= m_check_every_n_appends) {
      m_count = 0;
      time_t now = ::time(NULL);
      time_t cur_period = now / kRollPerSeconds * kRollPerSeconds;
      if (cur_period != m_start_time) {
        roll_file();
      } else if (now - m_last_flush > m_flush_interval) {
        m_last_flush = now;
        m_file->flush();
      }
    }
  }
}

bool LogFile::roll_file() {
  time_t now = 0;
  string filename = get_log_file_name(m_basename, &now);
  time_t start = now / kRollPerSeconds * kRollPerSeconds;

  if (now > m_last_roll) {
    m_last_roll = now;
    m_last_flush = now;
    m_start_time = start;
    m_file.reset(new FileUtil::AppendFile(filename));
    return true;
  }
  return false;
}

string LogFile::get_log_file_name(const string& basename, time_t* now) {
  string filename;
  filename.reserve(basename.size() + 64);
  filename = basename;

  char timebuf[32];
  struct tm tm;
  *now = time(NULL);
  gmtime_r(now, &tm);  // FIXME: localtime_r ?
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
  filename += timebuf;

  filename += ProcessInfo::hostname();

  char pidbuf[32];
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
  filename += pidbuf;

  filename += ".log";

  return filename;
}

}  // namespace flute