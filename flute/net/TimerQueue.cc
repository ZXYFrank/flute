#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <flute/net/TimerQueue.h>

#include <flute/common/LogLine.h>
#include <flute/net/Reactor.h>
#include <flute/net/Timer.h>
#include <flute/net/TimerId.h>

#include <sys/timerfd.h>
#include <unistd.h>

namespace flute {

namespace {  // anonymous

int create_timer_fd() {
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0) {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

struct timespec how_long_from_now(Timestamp when) {
  int64_t microseconds = when.micro_seconds_since_epoch() -
                         Timestamp::now().micro_seconds_since_epoch();
  if (microseconds < 100) {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec =
      static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}

void read_timer_fd(int timerfd, Timestamp now) {
  uint64_t howmany;
  // https://linux.die.net/man/3/read
  ssize_t n_bytes_read = ::read(timerfd, &howmany, sizeof howmany);
  LOG_TRACE << "read_timer_fd() " << howmany << " at " << now.to_string();
  if (n_bytes_read != sizeof howmany) {
    LOG_ERROR << "read_timer_fd() reads " << n_bytes_read
              << " bytes instead of 8";
  }
}

void reset_timer_fd(int timerfd, Timestamp expiration) {
  // wake up loop by timerfd_settime()
  struct itimerspec new_val;
  struct itimerspec old_val;
  mem_zero(&new_val, sizeof new_val);
  mem_zero(&old_val, sizeof old_val);
  // it_value for expiration
  new_val.it_value = how_long_from_now(expiration);
  int ret = ::timerfd_settime(timerfd, 0, &new_val, &old_val);
  if (ret) {
    LOG_SYSERR << "timerfd_settime()";
  }
}

}  // namespace

TimerQueue::TimerQueue(Reactor* reactor)
    : m_reactor(reactor),
      m_timer_fd(create_timer_fd()),
      m_timer_fd_channel(reactor, m_timer_fd),
      m_timers_set(),
      m_is_calling_expired_timers(false) {
  m_timer_fd_channel.set_read_callback(
      std::bind(&TimerQueue::handle_timerfd_read, this));
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  m_timer_fd_channel.wang_to_read();
}

TimerQueue::~TimerQueue() {
  m_timer_fd_channel.end_all();
  m_timer_fd_channel.remove_self_from_reactor();
  ::close(m_timer_fd);
  // do not remove channel, since we're in Reactor::dtor();
  // WARNING: raw pointers
  for (const Entry& timer : m_timers_set) {
    delete timer.second;
  }
}

TimerId TimerQueue::add_timer(TimerCallback cb, Timestamp when,
                              double interval) {
  Timer* timer = new Timer(std::move(cb), when, interval);
  m_reactor->run_asap_in_reactor(
      std::bind(&TimerQueue::add_timer_in_reactor, this, timer));
  return TimerId(timer, timer->global_id());
}

void TimerQueue::remove(TimerId timerId) {
  m_reactor->run_asap_in_reactor(
      std::bind(&TimerQueue::cancel_in_reactor, this, timerId));
}

void TimerQueue::add_timer_in_reactor(Timer* timer) {
  m_reactor->assert_in_reactor_thread();
  bool earliestChanged = insert(timer);

  if (earliestChanged) {
    reset_timer_fd(m_timer_fd, timer->expiration());
  }
}

void TimerQueue::cancel_in_reactor(TimerId timerId) {
  m_reactor->assert_in_reactor_thread();
  assert(m_timers_set.size() == m_active_timers_set.size());
  ActiveTimer timer(timerId.m_timer_ptr, timerId.sequence_);
  ActiveTimerSet::iterator it = m_active_timers_set.find(timer);
  if (it != m_active_timers_set.end()) {
    size_t n = m_timers_set.erase(Entry(it->first->expiration(), it->first));
    assert(n == 1);
    (void)n;
    delete it->first;  // FIXME: no delete please
    m_active_timers_set.erase(it);
  } else if (m_is_calling_expired_timers) {
    m_canceling_timers.insert(timer);
  }
  assert(m_timers_set.size() == m_active_timers_set.size());
}

// call back the
void TimerQueue::handle_timerfd_read() {
  m_reactor->assert_in_reactor_thread();
  Timestamp now(Timestamp::now());
  read_timer_fd(m_timer_fd, now);

  std::vector<Entry> expired = retrieve_expired_timers(now);

  m_is_calling_expired_timers = true;
  m_canceling_timers.clear();
  // safe to callback outside critical section
  for (const Entry& it : expired) {
    it.second->alarm();
  }
  m_is_calling_expired_timers = false;

  reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::retrieve_expired_timers(
    Timestamp now) {
  assert(m_timers_set.size() == m_active_timers_set.size());
  std::vector<Entry> expired;
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
  TimerSet::iterator end = m_timers_set.lower_bound(sentry);
  assert(end == m_timers_set.end() || now < end->first);
  std::copy(m_timers_set.begin(), end, back_inserter(expired));
  m_timers_set.erase(m_timers_set.begin(), end);

  for (const Entry& it : expired) {
    ActiveTimer timer(it.second, it.second->global_id());
    size_t n = m_active_timers_set.erase(timer);
    assert(n == 1);
    (void)n;
  }

  assert(m_timers_set.size() == m_active_timers_set.size());
  return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now) {
  Timestamp nextExpire;

  for (const Entry& it : expired) {
    ActiveTimer timer(it.second, it.second->global_id());
    if (it.second->is_repeat() &&
        m_canceling_timers.find(timer) == m_canceling_timers.end()) {
      it.second->restart(now);
      insert(it.second);
    } else {
      // FIXME move to a free list
      delete it.second;  // FIXME: no delete please
    }
  }

  if (!m_timers_set.empty()) {
    nextExpire = m_timers_set.begin()->second->expiration();
  }

  if (nextExpire.valid()) {
    reset_timer_fd(m_timer_fd, nextExpire);
  }
}

bool TimerQueue::insert(Timer* timer) {
  m_reactor->assert_in_reactor_thread();
  assert(m_timers_set.size() == m_active_timers_set.size());
  bool is_earliest_changed = false;
  Timestamp when = timer->expiration();
  TimerSet::iterator it = m_timers_set.begin();
  if (it == m_timers_set.end() || when < it->first) {
    is_earliest_changed = true;
  }
  {
    std::pair<TimerSet::iterator, bool> result =
        m_timers_set.insert(Entry(when, timer));
    assert(result.second);
    (void)result;
  }
  {
    std::pair<ActiveTimerSet::iterator, bool> result =
        m_active_timers_set.insert(ActiveTimer(timer, timer->global_id()));
    assert(result.second);
    (void)result;
  }

  assert(m_timers_set.size() == m_active_timers_set.size());
  return is_earliest_changed;
}

}  // namespace flute
