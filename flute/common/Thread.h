#ifndef FLUTE_COMMON_THREAD_H
#define FLUTE_COMMON_THREAD_H

#include <flute/common/Atomic.h>
#include <flute/common/noncopyable.h>
#include <flute/common/types.h>
#include <flute/common/LogLine.h>
#include <flute/common/CountdownLatch.h>

#include <pthread.h>
#include <functional>
#include <unistd.h>
#include <sys/syscall.h>

namespace flute {
class Thread : noncopyable {
 public:
  typedef std::function<void()> ThreadFunc;
  static AtomicInt32 g_threads_counter;
  enum ThreadStates {
    kCreated,
    kNotStarted,
    kHasJoined,
    kJoinSuccessful,
    kJoinFailed
  };

 private:
  pthread_t m_pthread_id;  // pthread_id in a process
  pid_t m_holder_tid;
  pid_t m_runner_tid;
  const char* m_basename;
  string m_name;
  ThreadFunc m_func;
  bool m_started;
  bool m_joined;
  // capsule mutex and cond for convenience
  CountdownLatch m_countdown_latch;

 public:
  // TODO: make it movable in C++11
  explicit Thread(ThreadFunc func, const char* = NULL);
  explicit Thread(ThreadFunc func, const string& str);
  ~Thread();

  void start();
  ThreadStates join();
  void run();
  static void* starter(void* param);

  pthread_t pthread_id() const { return m_pthread_id; }
  pid_t holder_tid() const { return m_holder_tid; }
  pid_t runner_tid() const { return m_runner_tid; }

  bool is_started() const { return m_started; }
  bool is_joined() const { return m_joined; }
  const string& name() const { return m_name; }
  static int threads_counter() { return g_threads_counter.get(); }

  static pid_t gettid() { return static_cast<pid_t>(::syscall(SYS_gettid)); }

 private:
  void update_name();
};

}  // namespace flute

#endif  // FLUTE_COMMON_THREAD_H