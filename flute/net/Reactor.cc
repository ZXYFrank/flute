

#include <flute/net/Reactor.h>

#include <flute/common/LogLine.h>
#include <flute/common/Mutex.h>
#include <flute/net/Channel.h>
#include <flute/net/Poller.h>
#include <flute/net/SocketsOps.h>
#include <flute/net/TimerQueue.h>

#include <algorithm>

#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace flute {

namespace {

// One reactor can belong to at most one thread.
__thread Reactor* t_reactor_of_this_thread = nullptr;

int create_event_fd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe {
 public:
  IgnoreSigPipe() {
    ::signal(SIGPIPE, SIG_IGN);
    // LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;
}  // namespace

static const int kPollTimeMs = 10000;

Reactor* Reactor::getReactorOfCurrentThread() {
  return t_reactor_of_this_thread;
}

Reactor::Reactor() : Reactor(kPollTimeMs) {}

Reactor::Reactor(int timeout_ms)
    : m_is_looping(false),
      m_going_to_quit(false),
      m_is_handling_event(false),
      m_is_calling_pending_tasks(false),
      m_iter_count(0),
      m_tid(CurrentThread::tid()),
      m_timeout_ms(timeout_ms),
      m_poller(Poller::new_default_poller(this)),
      m_timerqueue(new TimerQueue(this)),
      m_wakeup_fd(create_event_fd()),
      m_wakeup_channel(new Channel(this, m_wakeup_fd)),
      m_current_active_channel(NULL) {
  LOG_DEBUG << "Reactor created " << CurrentThread::name() << " in thread "
            << m_tid;
  if (t_reactor_of_this_thread) {
    LOG_FATAL << "Another Reactor " << t_reactor_of_this_thread
              << " exists in this thread " << m_tid;
  } else {
    t_reactor_of_this_thread = this;
  }
  m_wakeup_channel->set_read_callback(std::bind(&Reactor::handle_wakeup, this));
  // Only consider reading events. Event fd is always writable
  m_wakeup_channel->wang_to_read();
}

Reactor::~Reactor() {
  LOG_DEBUG << "Reactor " << CurrentThread::name() << " of thread " << m_tid
            << " destructs in thread " << CurrentThread::tid();
  m_wakeup_channel->end_all();
  m_wakeup_channel->remove_self_from_reactor();
  ::close(m_wakeup_fd);
  t_reactor_of_this_thread = NULL;
}

void Reactor::loop() {
  assert(!m_is_looping);
  assert_in_reactor_thread();
  m_is_looping = true;
  m_going_to_quit =
      false;  // FIXME: what if someone calls quit() before loop() ?
  LOG_TRACE << "Reactor " << CurrentThread::name() << " start looping";

  while (!m_going_to_quit) {
    m_active_channels.clear();
    m_poll_return_time = m_poller->poll(m_timeout_ms, &m_active_channels);
    ++m_iter_count;
    if (LogLine::get_log_level() <= LogLine::TRACE) {
      print_active_channels();
    }
    // TODO sort channel by priority
    m_is_handling_event = true;
    for (Channel* channel : m_active_channels) {
      m_current_active_channel = channel;
      m_current_active_channel->handle_event(m_poll_return_time);
    }
    m_current_active_channel = NULL;
    m_is_handling_event = false;
    do_queueing_tasks();
  }

  LOG_TRACE << "Reactor " << CurrentThread::name() << " stop looping";
  m_is_looping = false;
}

void Reactor::mark_quit() {
  m_going_to_quit = true;
  // There is a chance that loop() just executes while(!quit_) and exits,
  // then Reactor destructs, then we are accessing an invalid object.
  // Can be fixed using m_mutexlock in both places.
  if (!is_in_reactor_thread()) {
    wakeup_from_waiting_pool();
  }
}

// Submit a task(void()) to reactor and do it as soon as possible
// TODO: Reactor's responsibility is not clear.
// This functions assign the task back to the reactor. However, the reactor is
// actually only responsible for assigning tasks to ready channels. If we do
// want to the reactor wait for the poller IDLY, we need to estimate
// concurrency, i.e. the load of managing events in IO multiplexing.
void Reactor::run_asap_in_reactor(Task task_func) {
  if (is_in_reactor_thread()) {
    task_func();
  } else {
    queue_in_reactor(std::move(task_func));
  }
}

void Reactor::queue_in_reactor(Task task_func) {
  {
    MutexLockGuard lock(m_mutexlock);
    m_pending_tasks.push_back(std::move(task_func));
  }
  if (!is_in_reactor_thread() || m_is_calling_pending_tasks) {
    wakeup_from_waiting_pool();
  }
}

size_t Reactor::num_pending_tasks() const {
  MutexLockGuard lock(m_mutexlock);
  return m_pending_tasks.size();
}

TimerId Reactor::run_at(Timestamp time, TimerCallback cb) {
  return m_timerqueue->add_timer(std::move(cb), time, 0.0);
}

TimerId Reactor::run_after(double delay, TimerCallback cb) {
  Timestamp time(add_second(Timestamp::now(), delay));
  return run_at(time, std::move(cb));
}

TimerId Reactor::run_every(double interval, TimerCallback cb) {
  Timestamp time(add_second(Timestamp::now(), interval));
  return m_timerqueue->add_timer(std::move(cb), time, interval);
}

void Reactor::remove(TimerId timerId) { return m_timerqueue->remove(timerId); }

void Reactor::update_channel(Channel* channel) {
  assert(channel->m_owner_reactor() == this);
  assert_in_reactor_thread();
  m_poller->update_channel(channel);
}

void Reactor::remove_channel(Channel* channel) {
  assert(channel->m_owner_reactor() == this);
  assert_in_reactor_thread();
  if (m_is_handling_event) {
    assert(m_current_active_channel == channel ||
           std::find(m_active_channels.begin(), m_active_channels.end(),
                     channel) == m_active_channels.end());
  }
  m_poller->remove_channel(channel);
}

bool Reactor::has_channel(Channel* channel) {
  assert(channel->m_owner_reactor() == this);
  assert_in_reactor_thread();
  return m_poller->has_channel(channel);
}

void Reactor::abort_not_in_reactor_thread() {
  LOG_FATAL << "Reactor::abort_not_in_reactor_thread - Reactor "
            << CurrentThread::name() << " was created in threadId_ = " << m_tid
            << ", current thread id = " << CurrentThread::tid();
}

//
void Reactor::wakeup_from_waiting_pool() {
  uint64_t one = 1;
  ssize_t n = socket_ops::write(m_wakeup_fd, &one, sizeof one);
  if (n != sizeof one) {
    LOG_ERROR << "Reactor::wakeup_from_waiting_pool() writes " << n
              << " bytes instead of 8";
  }
}

void Reactor::handle_wakeup() {
  uint64_t one = 1;
  ssize_t n = socket_ops::read(m_wakeup_fd, &one, sizeof one);
  if (n != sizeof one) {
    LOG_ERROR << "Reactor::handle_read() reads " << n << " bytes instead of 8";
  }
}

void Reactor::do_queueing_tasks() {
  std::vector<Task> tasks;
  m_is_calling_pending_tasks = true;

  {
    MutexLockGuard lock(m_mutexlock);
    tasks.swap(m_pending_tasks);
  }

  for (const Task& functor : tasks) {
    functor();
  }
  m_is_calling_pending_tasks = false;
}

void Reactor::print_active_channels() const {
  for (const Channel* channel : m_active_channels) {
    LOG_TRACE << "{" << channel->recv_events_to_string() << "} ";
  }
}

}  // namespace flute