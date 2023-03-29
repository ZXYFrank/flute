#ifndef FLUTE_NET_TIMERQUEUE_H
#define FLUTE_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include <flute/common/Mutex.h>
#include <flute/common/Timestamp.h>
#include <flute/net/Callbacks.h>
#include <flute/net/Channel.h>

namespace flute {

class Reactor;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
///
class TimerQueue : noncopyable {
 public:
  explicit TimerQueue(Reactor* reactor);
  ~TimerQueue();

  ///
  /// Schedules the callback to be run at given time,
  /// repeats if @c interval > 0.0.
  ///
  /// Must be thread safe. Usually be called from other threads.
  TimerId add_timer(TimerCallback cb, Timestamp when, double interval);

  void remove(TimerId timerId);

 private:
  // FIXME: use unique_ptr<Timer> instead of raw pointers.
  // This requires heterogeneous comparison lookup (N3465) from C++14
  // so that we can find an T* in a set<unique_ptr<T>>.
  typedef std::pair<Timestamp, Timer*> Entry;
  typedef std::set<Entry> TimerSet;
  typedef std::pair<Timer*, int64_t> ActiveTimer;
  typedef std::set<ActiveTimer> ActiveTimerSet;

  void add_timer_in_reactor(Timer* timer);
  void cancel_in_reactor(TimerId timerId);
  // called when timerfd alarms
  void handle_timerfd_read();
  // move out all expired timers
  std::vector<Entry> retrieve_expired_timers(Timestamp now);
  void reset(const std::vector<Entry>& expired, Timestamp now);

  bool insert(Timer* timer);

  // FIXME: raw pointers
  Reactor* m_reactor;
  const int m_timer_fd;
  Channel m_timer_fd_channel;
  // Timer list sorted by expiration
  TimerSet m_timers_set;

  // for remove()
  ActiveTimerSet m_active_timers_set;
  bool m_is_calling_expired_timers; /* atomic */
  ActiveTimerSet m_canceling_timers;
};

}  // namespace flute
#endif  // FLUTE_NET_TIMERQUEUE_H
