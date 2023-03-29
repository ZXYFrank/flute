// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <flute/common/TaskDeque.h>

#include <flute/common/Exception.h>

#include <assert.h>
#include <stdio.h>

namespace flute {

TaskDeque::TaskDeque(const string& name_arg)
    : m_task_deque(kMaxTaskDequeSize),
      m_name(name_arg),
      m_is_running(false),
      m_cond_not_empty(m_deque_mutex),
      m_cond_not_full(m_deque_mutex) {}

TaskDeque::~TaskDeque() {
  LOG_TRACE << "TaskDeque dtor";
  if (m_is_running) {
    stop();
  }
}

void TaskDeque::start(int num_threads) {
  assert(m_threads.empty());
  m_is_running = true;
  m_threads.reserve(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    char id[32];
    snprintf(id, sizeof id, "%d", i + 1);
    m_threads.emplace_back(
        new Thread(std::bind(&TaskDeque::worker_loop, this), m_name + id));
    m_threads[i]->start();
  }
  if (num_threads == 0 && m_thread_init_callback) {
    m_thread_init_callback();
  }
}

void TaskDeque::stop() {
  {
    MutexLockGuard lock(m_deque_mutex);
    m_is_running = false;
    m_cond_not_empty.notify_all();
  }
  for (auto& thr : m_threads) {
    thr->join();
  }
}

size_t TaskDeque::size() const {
  MutexLockGuard lock(m_deque_mutex);
  return m_task_deque.size();
}

size_t TaskDeque::max_deque_size() const { return m_max_queue_size; }

void TaskDeque::push_task(Task task, bool push_back) {
  if (m_threads.empty()) {
    // call the task in current thread.
    task();
  } else {
    MutexLockGuard lock(m_deque_mutex);
    while (is_full()) {
      m_cond_not_full.wait();
    }
    assert(!is_full());

    if (push_back) {
      m_task_deque.push_back(std::move(task));
    } else {
      m_task_deque.push_front(std::move(task));
    }
    m_cond_not_empty.notify();
  }
}

TaskDeque::Task TaskDeque::take_one() {
  MutexLockGuard lock(m_deque_mutex);
  // always use a while-loop, due to spurious wakeup
  while (m_task_deque.empty() && m_is_running) {
    m_cond_not_empty.wait();
  }
  Task task;
  if (!m_task_deque.empty()) {
    task = m_task_deque.front();
    m_task_deque.pop_front();
    if (m_max_queue_size > 0) {
      m_cond_not_full.notify();
    }
  }
  return task;
}

bool TaskDeque::is_full() const {
  m_deque_mutex.assert_locked_by_this_thread();
  return m_max_queue_size > 0 && m_task_deque.size() >= m_max_queue_size;
}

// worker threads will repeat sleeping to be notified by the condition, taking
// one task from the queue(guarded by mutex), executing it, and then sleeping on
// the cond again.
void TaskDeque::worker_loop() {
  try {
    if (m_thread_init_callback) {
      m_thread_init_callback();
    }
    while (m_is_running) {
      Task task(take_one());
      if (task) {
        task();
      }
    }
  } catch (const Exception& ex) {
    fprintf(stderr, "exception caught in TaskDeque %s\n", m_name.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  } catch (const std::exception& ex) {
    fprintf(stderr, "exception caught in TaskDeque %s\n", m_name.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  } catch (...) {
    fprintf(stderr, "unknown exception caught in TaskDeque %s\n",
            m_name.c_str());
    throw;  // rethrow
  }
}

}  // namespace flute