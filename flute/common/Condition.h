#ifndef FLUTE_COMMON_CONDITION_H
#define FLUTE_COMMON_CONDITION_H

#include <flute/common/Mutex.h>
#include <flute/common/common.h>

#include <pthread.h>

namespace flute {

class Condition : noncopyable {
 private:
  MutexLock& m_mutexlock;
  pthread_cond_t m_pthread_cond;

 public:
  explicit Condition(MutexLock& mutex) : m_mutexlock(mutex) {
    MCHECK(pthread_cond_init(&m_pthread_cond, NULL));
  }
  ~Condition() { MCHECK(pthread_cond_destroy(&m_pthread_cond)); };

  void wait();
  bool wait_for_seconds(double seconds);
  void notify() { MCHECK(pthread_cond_signal(&m_pthread_cond)); }
  void notify_all() { MCHECK(pthread_cond_broadcast(&m_pthread_cond)); }
};
}  // namespace flute

#endif  // FLUTE_COMMON_CONDITION_H