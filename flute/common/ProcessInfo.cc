#include <flute/common/ProcessInfo.h>
#include <flute/common/CurrentThread.h>
#include <flute/common/FileUtil.h>
#include <flute/common/types.h>

#include <algorithm>

#include <assert.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>  // snprintf
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/times.h>

namespace flute {
namespace detail {
__thread int t_num_opened_files = 0;
int fdDirFilter(const struct dirent* d) {
  if (::isdigit(d->d_name[0])) {
    ++t_num_opened_files;
  }
  return 0;
}

__thread std::vector<pid_t>* t_pids = NULL;
int taskDirFilter(const struct dirent* d) {
  if (::isdigit(d->d_name[0])) {
    t_pids->push_back(atoi(d->d_name));
  }
  return 0;
}

int scanDir(const char* dirpath, int (*filter)(const struct dirent*)) {
  struct dirent** namelist = NULL;
  int result = ::scandir(dirpath, &namelist, filter, alphasort);
  assert(namelist == NULL);
  return result;
}

Timestamp g_start_time = Timestamp::now();
// assume those won't change during the life time of a process.
int g_clockTicks = static_cast<int>(::sysconf(_SC_CLK_TCK));
int g_pageSize = static_cast<int>(::sysconf(_SC_PAGE_SIZE));
}  // namespace detail
}  // namespace flute

using namespace flute;
using namespace flute::detail;

pid_t ProcessInfo::pid() { return ::getpid(); }

string ProcessInfo::pidString() {
  char buf[32];
  snprintf(buf, sizeof buf, "%d", pid());
  return buf;
}

uid_t ProcessInfo::uid() { return ::getuid(); }

string ProcessInfo::username() {
  struct passwd pwd;
  struct passwd* result = NULL;
  char buf[8192];
  const char* name = "unknownuser";

  getpwuid_r(uid(), &pwd, buf, sizeof buf, &result);
  if (result) {
    name = pwd.pw_name;
  }
  return name;
}

uid_t ProcessInfo::euid() { return ::geteuid(); }

Timestamp ProcessInfo::startTime() { return g_start_time; }

int ProcessInfo::clockTicksPerSecond() { return g_clockTicks; }

int ProcessInfo::pageSize() { return g_pageSize; }

bool ProcessInfo::isDebugBuild() {
#ifdef NDEBUG
  return false;
#else
  return true;
#endif
}

string ProcessInfo::hostname() {
  // HOST_NAME_MAX 64
  // _POSIX_HOST_NAME_MAX 255
  char buf[256];
  if (::gethostname(buf, sizeof buf) == 0) {
    buf[sizeof(buf) - 1] = '\0';
    return buf;
  } else {
    return "unknownhost";
  }
}

string ProcessInfo::procname() { return procname(procStat()).as_string(); }

StringPiece ProcessInfo::procname(const string& stat) {
  StringPiece name;
  size_t lp = stat.find('(');
  size_t rp = stat.rfind(')');
  if (lp != string::npos && rp != string::npos && lp < rp) {
    name = StringPiece(stat.data() + lp + 1, rp - lp - 1);
  }
  return name;
}

string ProcessInfo::procStatus() {
  string result;
  FileUtil::read_file("/proc/self/status", 65536, &result);
  return result;
}

string ProcessInfo::procStat() {
  string result;
  FileUtil::read_file("/proc/self/stat", 65536, &result);
  return result;
}

string ProcessInfo::threadStat() {
  char buf[64];
  snprintf(buf, sizeof buf, "/proc/self/task/%d/stat", CurrentThread::tid());
  string result;
  FileUtil::read_file(buf, 65536, &result);
  return result;
}

string ProcessInfo::exePath() {
  string result;
  char buf[1024];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf);
  if (n > 0) {
    result.assign(buf, n);
  }
  return result;
}

int ProcessInfo::openedFiles() {
  t_num_opened_files = 0;
  scanDir("/proc/self/fd", fdDirFilter);
  return t_num_opened_files;
}

int ProcessInfo::maxOpenFiles() {
  struct rlimit rl;
  if (::getrlimit(RLIMIT_NOFILE, &rl)) {
    return openedFiles();
  } else {
    return static_cast<int>(rl.rlim_cur);
  }
}

ProcessInfo::CpuTime ProcessInfo::cpuTime() {
  ProcessInfo::CpuTime t;
  struct tms tms;
  if (::times(&tms) >= 0) {
    const double hz = static_cast<double>(clockTicksPerSecond());
    t.userSeconds = static_cast<double>(tms.tms_utime) / hz;
    t.systemSeconds = static_cast<double>(tms.tms_stime) / hz;
  }
  return t;
}

int ProcessInfo::numThreads() {
  int result = 0;
  string status = procStatus();
  size_t pos = status.find("Threads:");
  if (pos != string::npos) {
    result = ::atoi(status.c_str() + pos + 8);
  }
  return result;
}

std::vector<pid_t> ProcessInfo::threads() {
  std::vector<pid_t> result;
  t_pids = &result;
  scanDir("/proc/self/task", taskDirFilter);
  t_pids = NULL;
  std::sort(result.begin(), result.end());
  return result;
}
