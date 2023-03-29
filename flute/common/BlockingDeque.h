#ifndef FLUTE_COMMON_BLOCKINGDEQUE_H
#define FLUTE_COMMON_BLOCKINGDEQUE_H

#include <flute/common/Condition.h>
#include <flute/common/Mutex.h>

#include <deque>
#include <assert.h>

namespace flute {

template <typename T>
class BlockingDeque : noncopyable {
 public:
  BlockingDeque() : m_mutex(), m_cond_not_empty(m_mutex), m_deque() {}

  void push_back(const T& x) { push(x, true); }
  void push_front(const T& x) { push(x, false); }

  T take_front() {
    MutexLockGuard lock(m_mutex);
    // always use a while-loop, due to spurious wakeup
    while (m_deque.is_empty()) {
      m_cond_not_empty.wait();
    }
    assert(!m_deque.is_empty());
    T front(std::move(m_deque.front()));
    m_deque.pop_front();
    return front;
  }

  size_t size() const {
    MutexLockGuard lock(m_mutex);
    return m_deque.size();
  }

 private:
  void push(const T& x, bool push_back) {
    MutexLockGuard lock(m_mutex);
    if (push_back) {
      m_deque.push_back(x);
    } else {
      m_deque.push_front(x);
    }
    m_cond_not_empty.notify();  // wait morphing saves us
    // http://www.domaigne.com/blog/computing/condvars-signal-with-mutex-locked-or-not/
  }
  mutable MutexLock m_mutex;
  Condition m_cond_not_empty GUARDED_BY(mutex_);
  std::deque<T> m_deque GUARDED_BY(mutex_);
};

}  // namespace flute

#endif  // FLUTE_COMMON_BLOCKINGDEQUE_H
