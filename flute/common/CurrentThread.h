

#ifndef FLUTE_COMMON_CURRENTTHREAD_H
#define FLUTE_COMMON_CURRENTTHREAD_H

#include <flute/common/types.h>

namespace flute {
namespace CurrentThread {
extern __thread int t_cached_tid;
extern __thread char t_tid_string[32];
extern __thread int t_tid_string_length;
extern __thread const char* t_thread_name;
void cache_tid();

inline int tid() {
  if (LIKELY_FALSE(t_cached_tid == 0)) {
    cache_tid();
  }
  return t_cached_tid;
}

inline const char* tid_string() {
  if (LIKELY_FALSE(t_cached_tid == 0)) {
    cache_tid();
  }
  return t_tid_string;
}

inline int tid_string_length() {
  if (LIKELY_FALSE(t_cached_tid == 0)) {
    cache_tid();
  }
  return t_tid_string_length;
}

inline const char* name() { return t_thread_name; }
inline void set_name(const char* name) { t_thread_name = name; }

bool is_main_thread();

void sleep_usec(int64_t usec);  // for testing

string stacktrace(bool demangle);
}  // namespace CurrentThread
}  // namespace flute

#endif  // FLUTE_COMMON_CURRENTTHREAD_H
