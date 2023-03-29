#include <errno.h>
#include <flute/common/CurrentThread.h>
#include <flute/common/Exception.h>
#include <flute/common/Thread.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <type_traits>

namespace flute {

AtomicInt32 Thread::g_threads_counter;

Thread::Thread(ThreadFunc func, const string& str)
    : Thread(func, str.c_str()) {}

Thread::Thread(ThreadFunc func, const char* name)
    : m_func(std::move(func)),
      m_joined(false),
      m_basename(name),
      m_started(false),
      m_pthread_id(0),
      m_holder_tid(CurrentThread::tid()),
      m_runner_tid(0),
      m_countdown_latch(1) {
  update_name();
}

void Thread::update_name() {
  pid_t __tid = m_started ? m_runner_tid : m_holder_tid;
  if (m_basename == NULL) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Thread[%d]", __tid);
    m_name = std::move(string(buf));
  } else {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s[%d]", m_basename, __tid);
    m_name = std::move(string(buf));
  }
  if (!m_started) {
    m_name += "[NotStarted]";
  }
}

// For those started threads joined yet, we leave work of freeing resources to
// them.
Thread::~Thread() {
  if (m_started && !m_joined) {
    pthread_detach(m_pthread_id);
  }
  LOG_TRACE << m_name << " deconstructed";
}

void* Thread::starter(void* param) {
  Thread* thread = static_cast<Thread*>(param);
  thread->m_runner_tid = CurrentThread::tid();
  thread->run();
  return NULL;
}

void Thread::run() {
  update_name();
  flute::CurrentThread::set_name(m_name.empty() ? "DefaultThread"
                                                : m_name.c_str());
  ::prctl(PR_SET_NAME, flute::CurrentThread::t_thread_name);
  try {
    // Here, we notify the parent thread that the child want to start func.
    m_countdown_latch.countdown();
    LOG_TRACE << name() << " run!";
    m_func();
    flute::CurrentThread::t_thread_name = "FINISHED";
  } catch (const flute::Exception& ex) {
    flute::CurrentThread::t_thread_name = "CRASHED";
    fprintf(stderr, "exception caught in Thread %s\n", m_name.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  } catch (const std::exception& ex) {
    flute::CurrentThread::t_thread_name = "CRASHED";
    fprintf(stderr, "exception caught in Thread %s\n", m_name.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  } catch (...) {
    flute::CurrentThread::t_thread_name = "CRASHED";
    fprintf(stderr, "unknown exception caught in Thread %s\n", m_name.c_str());
    throw;  // rethrow
  }
}

void Thread::start() {
  assert(!m_started);
  m_started = true;

  if (pthread_create(&m_pthread_id, NULL, Thread::starter, this)) {
    m_started = false;
    LOG_SYSFATAL << "Thread " << Thread::g_threads_counter.get()
                 << " Failed in pthread_create";
  } else {
    // pthread_create succeeded.
    // Here in main thread, it wait the sub-thread to notify that it has
    // started.
    m_countdown_latch.wait();
    LOG_TRACE << "Holder " << m_holder_tid << " got notification: tid["
              << name() << "] created";
    // LOG_TRACE << "created tid = " << m_global_tid;
  }
}

Thread::ThreadStates Thread::join() {
  if (!m_started)
    return kNotStarted;
  else if (m_joined)
    return kHasJoined;
  else {
    m_joined = true;
    LOG_TRACE << name() << " calls join()";
    return pthread_join(m_pthread_id, NULL) == 0 ? kJoinSuccessful
                                                 : kJoinFailed;
  }
}

bool CurrentThread::is_main_thread() { return tid() == ::getpid(); }

void CurrentThread::sleep_usec(int64_t usec) {
  struct timespec ts = {0, 0};
  ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec =
      static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
  ::nanosleep(&ts, NULL);
}

}  // namespace flute
