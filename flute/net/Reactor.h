
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_Reactor_H
#define FLUTE_NET_Reactor_H

#include <any>
#include <atomic>
#include <functional>
#include <vector>

#include <flute/common/Mutex.h>
#include <flute/common/CurrentThread.h>
#include <flute/common/Timestamp.h>
#include <flute/net/Callbacks.h>
#include <flute/net/TimerId.h>

namespace flute {

class Channel;
class Poller;
class TimerQueue;

///
/// Reactor, at most one per thread.
/// Loop for receiving events from pollers and the submitted tasks functors.
///
/// This is an interface class, so don't expose too much details.
class Reactor : noncopyable {
 public:
  typedef std::function<void()> Task;

  Reactor();
  Reactor(int timeout_ms);
  ~Reactor();  // force out-line dtor, for std::unique_ptr members.

  ///
  /// Loops forever.
  ///
  /// Must be called in the same thread as creation of the object.
  ///
  void loop();

  /// Quits loop.
  ///
  /// This is not 100% thread safe, if you call through a raw pointer,
  /// better to call through shared_ptr<Reactor> for 100% safety.
  void mark_quit();

  ///
  /// Time when poll returns, usually means data arrival.
  ///
  Timestamp poll_return_time() const { return m_poll_return_time; }

  int64_t iteration() const { return m_iter_count; }

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
  void run_asap_in_reactor(Task task_func);
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void queue_in_reactor(Task task_func);

  size_t num_pending_tasks() const;

  // timers

  ///
  /// Runs callback at 'time'.
  /// Safe to call from other threads.
  ///
  TimerId run_at(Timestamp time, TimerCallback cb);
  ///
  /// Runs callback after @c delay seconds.
  /// Safe to call from other threads.
  ///
  TimerId run_after(double delay, TimerCallback cb);
  ///
  /// Runs callback every @c interval seconds.
  /// Safe to call from other threads.
  ///
  TimerId run_every(double interval, TimerCallback cb);
  ///
  /// Cancels the timer.
  /// Safe to call from other threads.
  ///
  void remove(TimerId timerId);

  // internal usage
  void wakeup_from_waiting_pool();
  void update_channel(Channel* channel);
  void remove_channel(Channel* channel);
  bool has_channel(Channel* channel);

  // pid_t threadId() const { return threadId_; }
  void assert_in_reactor_thread() {
    if (!is_in_reactor_thread()) {
      abort_not_in_reactor_thread();
    }
  }
  bool is_in_reactor_thread() const { return m_tid == CurrentThread::tid(); }
  // bool callingPendingFunctors() const { return callingPendingFunctors_; }
  bool is_handling_event() const { return m_is_handling_event; }

  void set_context(const void* context) { m_context = context; }

  const void* get_context_ptr() const { return m_context; }

  void* get_mutable_context_ptr() { return &m_context; }

  static Reactor* getReactorOfCurrentThread();

 private:
  void abort_not_in_reactor_thread();
  void handle_wakeup();  // waked up
  void do_queueing_tasks();

  void print_active_channels() const;  // DEBUG

  typedef std::vector<Channel*> ChannelPtrList;

  bool m_is_looping; /* atomic */
  std::atomic<bool> m_going_to_quit;
  bool m_is_handling_event;        /* atomic */
  bool m_is_calling_pending_tasks; /* atomic */
  int64_t m_iter_count;
  const pid_t m_tid;

  const int m_timeout_ms;
  Timestamp m_poll_return_time;
  std::unique_ptr<Poller> m_poller;
  std::unique_ptr<TimerQueue> m_timerqueue;
  int m_wakeup_fd;
  // a standalone channel responsible for the waking up events
  std::unique_ptr<Channel> m_wakeup_channel;
  const void* m_context;

  // scratch variables
  ChannelPtrList m_active_channels;
  Channel* m_current_active_channel;

  mutable MutexLock m_mutexlock;
  std::vector<Task> m_pending_tasks GUARDED_BY(m_mutexlock);
};

}  // namespace flute

#endif  // FLUTE_NET_Reactor_H
