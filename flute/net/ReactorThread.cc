

#include <flute/net/ReactorThread.h>

#include <flute/net/Reactor.h>

using namespace flute;

ReactorThread::ReactorThread(const ThreadInitFunctor& cb, const string& name)
    : m_reactor(NULL),
      m_is_exiting(false),
      m_thread(std::bind(&ReactorThread::reactor_thread_loop_func, this), name),
      m_mutexlock(),
      m_cond(m_mutexlock),
      m_reactor_init_func(cb) {}

ReactorThread::~ReactorThread() {
  m_is_exiting = true;
  if (m_reactor != NULL)  // not 100% race-free, eg. reactor_thread_func could
                          // be running m_callback.
  {
    // still a tiny chance to call destructed object, if reactor_thread_func
    // exits just now. but when ReactorThread destructs, usually programming is
    // exiting anyway.
    m_reactor->mark_quit();
    m_thread.join();
  }
}

Reactor* ReactorThread::start_reactor() {
  if (m_thread.is_started()) {
    LOG_WARN << m_thread.name() << " has started.";
    return m_reactor;
  };
  m_thread.start();
  Reactor* reactor = NULL;
  {
    // m_thread(child thread) will notify the owner thread(reactor) that it has
    // started
    MutexLockGuard lock(m_mutexlock);
    while (m_reactor == NULL) {
      m_cond.wait();
    }
    reactor = m_reactor;
  }

  return reactor;
}

void ReactorThread::reactor_thread_loop_func() {
  Reactor loop;
  if (m_reactor_init_func) {
    m_reactor_init_func(&loop);
  }
  {
    MutexLockGuard lock(m_mutexlock);
    m_reactor = &loop;
    m_cond.notify();
  }
  // loop forever
  loop.loop();
  // assert(m_is_exiting);
  MutexLockGuard lock(m_mutexlock);
  m_reactor = NULL;
}
