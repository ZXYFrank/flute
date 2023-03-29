// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef FLUTE_COMMON_LOGFILE_H
#define FLUTE_COMMON_LOGFILE_H

#include <flute/common/Mutex.h>
#include <flute/common/types.h>

#include <memory>

namespace flute {

namespace FileUtil {
class AppendFile;
}

class LogFile : noncopyable {
 public:
  LogFile(const string& basename, off_t rollSize, bool threadSafe = true,
          int flushInterval = 3, int checkEveryN = 1024);
  ~LogFile();

  void append(const char* logline, int len);
  void flush();
  bool roll_file();

 private:
  void append_unlocked(const char* logline, int len);

  static string get_log_file_name(const string& basename, time_t* now);

  const string m_basename;
  const off_t m_rollsize;
  const int m_flush_interval;
  const int m_check_every_n_appends;

  int m_count;

  std::unique_ptr<MutexLock> m_mutex;
  time_t m_start_time;
  time_t m_last_roll;
  time_t m_last_flush;
  std::unique_ptr<FileUtil::AppendFile> m_file;

  static const int kRollPerSeconds = 60 * 60 * 24;
};

}  // namespace flute
#endif  // FLUTE_COMMON_LOGFILE_H
