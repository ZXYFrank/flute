#ifndef FLUTE_COMMON_TASKDEQUE_H
#define FLUTE_COMMON_TASKDEQUE_H

#include <flute/common/Condition.h>
#include <flute/common/Mutex.h>
#include <flute/common/Thread.h>
#include <flute/common/types.h>
#include <flute/common/BoundedBlockingDeque.h>

#include <deque>
#include <vector>

namespace flute {

class TaskDeque : noncopyable {
 public:
  static const int kMaxTaskDequeSize = 10;

  typedef std::function<void()> Task;

  explicit TaskDeque(const string& name_arg = string("TaskDeque"));
  ~TaskDeque();

  // Must be called before start().
  void set_max_deque_size(int maxsize) { m_max_queue_size = maxsize; }
  size_t max_deque_size() const;
  void set_thread_init_callback(const Task& cb) { m_thread_init_callback = cb; }

  void start(int num_threads);
  void stop();
  void wait() const {while (m_is_running);}

  const string& name() const { return m_name; }

  size_t size() const;

  void push_task_back(Task f) { push_task(f, true); };
  void push_task_front(Task f) { push_task(f, false); }

 private:
  // Could block if maxQueueSize > 0
  void push_task(Task f, bool push_back);
  bool is_full() const REQUIRES(m_queue_mutex);
  void worker_loop();
  Task take_one();

  mutable MutexLock m_deque_mutex;
  Condition m_cond_not_empty GUARDED_BY(m_deque_mutex);
  Condition m_cond_not_full GUARDED_BY(m_deque_mutex);
  string m_name;
  Task m_thread_init_callback;
  std::vector<std::unique_ptr<flute::Thread>> m_threads;
  std::deque<Task> m_task_deque GUARDED_BY(m_deque_mutex);
  size_t m_max_queue_size;
  bool m_is_running;
};

}  // namespace flute

#endif  // FLUTE_COMMON_TASKDEQUE_H
