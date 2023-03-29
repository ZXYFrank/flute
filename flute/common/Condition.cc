#include <flute/common/Condition.h>

#include <errno.h>

namespace flute {

bool Condition::wait_for_seconds(double seconds) {
  int64_t nanoseconds =
      static_cast<int64_t>(seconds * flute::kNanoSecondsPerSecond);
  struct timespec abstime;
  clock_gettime(CLOCK_MONOTONIC_RAW, &abstime);
  abstime.tv_sec += static_cast<time_t>((abstime.tv_nsec + nanoseconds) /
                                        flute::kNanoSecondsPerSecond);
  abstime.tv_nsec = static_cast<long>((abstime.tv_nsec + nanoseconds) %
                                      flute::kNanoSecondsPerSecond);

  MutexLock::UnassignHolderProcess us(m_mutexlock);
  return ETIMEDOUT ==
         pthread_cond_timedwait(&m_pthread_cond,
                                m_mutexlock.get_pthread_mutex_ptr(), &abstime);
}

void Condition::wait() {
  // The mutex inside m_mutexlock will be handed over to pthread_cond_wait until
  // it returns. So it temporally refuse to maintain the holder process id.
  MutexLock::UnassignHolderProcess us(m_mutexlock);
  MCHECK(
      pthread_cond_wait(&m_pthread_cond, m_mutexlock.get_pthread_mutex_ptr()));
}
}  // namespace flute
