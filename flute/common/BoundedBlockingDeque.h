#ifndef FLUTE_COMMON_BOUNDEDBLOCKINGDEQUE_H
#define FLUTE_COMMON_BOUNDEDBLOCKINGDEQUE_H

#include <flute/common/Condition.h>
#include <flute/common/Mutex.h>

#include <deque>
#include <assert.h>
#include <functional>

namespace flute {

typedef std::function<void()> Task;

template <typename T>
class BoundedBlockingDeque : noncopyable {
 public:
  static const int kDefaultDequeSize = 10;
  BoundedBlockingDeque(size_t max_deque_size = kDefaultDequeSize)
      : m_max_queue_size(max_deque_size),
        m_id_counter(0),
        m_queue_mutex(),
        m_cond_not_empty(m_queue_mutex),
        m_cond_not_full(m_queue_mutex) {}

  T take_front(int* id_ptr) {
    MutexLockGuard lock(m_queue_mutex);
    while (m_deque.empty()) {
      m_cond_not_empty.wait();
    }
    assert(!m_deque.empty());
    Element front(m_deque.front());
    m_deque.pop_front();
    m_cond_not_full.notify();
    if (id_ptr != nullptr) {
      *id_ptr = front.first;
    }
    LOG_TRACE << "Deque front id=" << front.first;
    return front.second;
  }
  T take_front() { return take_front(nullptr); }

  void push_back(const T& x) { push(x, true); }
  void push_front(const T& x) { push(x, false); }

  bool is_empty() const {
    MutexLockGuard lock(m_queue_mutex);
    return m_deque.is_empty();
  }

  bool is_full() const {
    MutexLockGuard lock(m_queue_mutex);
    return m_deque.size() == m_max_queue_size;
  }

  size_t size() const {
    MutexLockGuard lock(m_queue_mutex);
    return m_deque.size();
  }

  size_t capacity() const {
    MutexLockGuard lock(m_queue_mutex);
    return m_deque.capacity();
  }

  void set_size(size_t size) {
    MutexLockGuard lock(m_queue_mutex);
    m_max_queue_size = size;
  }

 private:
  void push(const T& x, bool push_back) {
    MutexLockGuard lock(m_queue_mutex);
    while (m_deque.size() == m_max_queue_size) {
      m_cond_not_full.wait();
    }
    assert(m_deque.size() < m_max_queue_size);
    m_id_counter++;
    Element e(m_id_counter, x);
    if (push_back) {
      m_deque.push_back(e);
    } else {
      m_deque.push_front(e);
    };
    m_cond_not_empty.notify();
  }

  size_t m_max_queue_size;
  size_t m_id_counter;
  mutable MutexLock m_queue_mutex;
  Condition m_cond_not_empty GUARDED_BY(m_queue_mutex);
  Condition m_cond_not_full GUARDED_BY(m_queue_mutex);
  // <id, value>
  typedef std::pair<int, T> Element;
  std::deque<Element> m_deque GUARDED_BY(m_queue_mutex);
};

}  // namespace flute

#endif  // FLUTE_COMMON_BOUNDEDBLOCKINGDEQUE_H
