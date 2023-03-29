

#include <flute/net/ReactorThreadPool.h>

#include <flute/net/Reactor.h>
#include <flute/net/ReactorThread.h>

#include <stdio.h>

using namespace flute;

ReactorThreadPool::ReactorThreadPool(Reactor* baseLoop, const string& name_arg)
    : m_default_reactor(baseLoop),
      m_name(name_arg),
      m_has_started(false),
      m_size_pool(0),
      m_next_idx(0) {}

ReactorThreadPool::~ReactorThreadPool() {
  // Don't delete loop, it's stack variable
}

void ReactorThreadPool::start(const ThreadInitFunctor& cb) {
  assert(!m_has_started);
  m_default_reactor->assert_in_reactor_thread();

  for (int i = 0; i < m_size_pool; ++i) {
    char buf[m_name.size() + 32];
    snprintf(buf, sizeof buf, "%s%d", m_name.c_str(), i);
    ReactorThread* t = new ReactorThread(cb, buf);
    // let the smart pointer manage the reactor thread.
    m_reactor_thread_pool.push_back(std::unique_ptr<ReactorThread>(t));
    m_reactors.push_back(t->start_reactor());
  }
  if (m_size_pool == 0 && cb) {
    cb(m_default_reactor);
  }

  m_has_started = true;
}

Reactor* ReactorThreadPool::get_next_reactor() {
  m_default_reactor->assert_in_reactor_thread();
  assert(m_has_started);
  Reactor* reactor = m_default_reactor;

  if (!m_reactors.empty()) {
    // round-robin
    reactor = m_reactors[m_next_idx];
    m_next_idx = static_cast<int>(((m_next_idx) + 1) % m_reactors.size());
  }
  return reactor;
}

Reactor* ReactorThreadPool::get_reactor_for_hashcode(size_t hash) {
  m_default_reactor->assert_in_reactor_thread();
  Reactor* reactor = m_default_reactor;

  if (!m_reactors.empty()) {
    reactor = m_reactors[hash % m_reactors.size()];
  }
  return reactor;
}

std::vector<Reactor*> ReactorThreadPool::get_all_reactors() {
  m_default_reactor->assert_in_reactor_thread();
  assert(m_has_started);
  if (m_reactors.empty()) {
    return std::vector<Reactor*>(1, m_default_reactor);
  } else {
    return m_reactors;
  }
}
