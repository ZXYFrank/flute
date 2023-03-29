#ifndef FLUTE_NET_TIMER_H
#define FLUTE_NET_TIMER_H

#include <flute/common/Atomic.h>
#include <flute/common/Timestamp.h>
#include <flute/net/Callbacks.h>

namespace flute {

///
/// Internal class for timer event.
///
class Timer : noncopyable {
 public:
  Timer(TimerCallback cb, Timestamp when, double interval)
      : m_callback(std::move(cb)),
        m_time_expire(when),
        m_interval(interval),
        m_is_repeated(interval > 0.0),
        m_global_timer_id(g_global_timer_id.increment_and_get()) {}

  void alarm() const { m_callback(); }

  Timestamp expiration() const { return m_time_expire; }
  bool is_repeat() const { return m_is_repeated; }
  int64_t global_id() const { return m_global_timer_id; }

  void restart(Timestamp now);

  static int64_t total_timer_count() { return g_global_timer_id.get(); }

 private:
  const TimerCallback m_callback;
  Timestamp m_time_expire;
  const double m_interval;
  const bool m_is_repeated;
  const int64_t m_global_timer_id;

  static AtomicInt64 g_global_timer_id;
};

}  // namespace flute

#endif  // FLUTE_NET_TIMER_H
