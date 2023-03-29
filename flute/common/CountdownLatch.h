#ifndef FLUTE_COMMON_COUNTDOWNLATCH_H
#define FLUTE_COMMON_COUNTDOWNLATCH_H

#include <flute/common/Mutex.h>
#include <flute/common/Condition.h>

namespace flute {
class CountdownLatch {
 private:
  MutexLock m_lock;
  Condition m_cond;
  int m_count;

 public:
  explicit CountdownLatch(int count)
      : m_count{count}, m_lock(), m_cond(m_lock) {}

  void wait() {
    MutexLockGuard lock(m_lock);
    while (m_count > 0) {
      m_cond.wait();
    }
  }

  void countdown() {
    MutexLockGuard lock(m_lock);
    m_count--;
    if (m_count == 0) m_cond.notify_all();
  }
};

}  // namespace flute

#endif  // FLUTE_COMMON_COUNTDOWNLATCH_H