#include <flute/common/LogLine.h>
#include <flute/net/Channel.h>
#include <flute/net/Reactor.h>
#include <poll.h>

#include <sstream>

namespace flute {

Channel::Channel(Reactor* reactor, int fd__)
    : m_reactor(reactor),
      m_fd(fd__),
      m_concerned_events(0),
      m_recv_events(0),
      m_idx(-1),
      m_loghup_enabled(true),
      m_tied(false),
      m_is_handing_event(false),
      m_is_in_reactor(false) {}

Channel::~Channel() {
  assert(!m_is_handing_event);
  assert(!m_is_in_reactor);
  if (m_reactor->is_in_reactor_thread()) {
    assert(!m_reactor->has_channel(this));
  }
}

// Tie this channel to the owner object managed by shared_ptr, prevent the owner
// object being destroyed in handle_event.
void Channel::tie(const std::shared_ptr<void>& obj) {
  m_tied_obj_ptr = obj;
  m_tied = true;
}

// WARNING: strongly coupled
void Channel::update_self_in_reactor() {
  m_is_in_reactor = true;
  m_reactor->update_channel(this);
}

void Channel::remove_self_from_reactor() {
  assert(is_none_event());
  m_is_in_reactor = false;
  m_reactor->remove_channel(this);
}

void Channel::handle_event(Timestamp recv_time) {
  // FIXME: m_tied may still be deconstructed or freed outside this function, if
  // the tied object is accessed with raw pointers.
  std::shared_ptr<void> guard;
  if (m_tied) {
    // QUESTION: converted to a shared pointer?
    guard = m_tied_obj_ptr.lock();
    if (guard) {
      handle_event_with_guard(recv_time);
    }
  } else {
    handle_event_with_guard(recv_time);
  }
}

// Core function of channel. Channel is aware of
void Channel::handle_event_with_guard(Timestamp recv_time) {
  m_is_handing_event = true;
  LOG_TRACE << recv_events_to_string();
  if ((m_recv_events & POLLHUP) && !(m_recv_events & POLLIN)) {
    if (m_loghup_enabled) {
      LOG_WARN << "fd = " << m_fd << " Channel::handle_event() POLLHUP";
    }
    if (m_close_callback) m_close_callback();
  }

  if (m_recv_events & POLLNVAL) {
    LOG_WARN << "fd = " << m_fd << " Channel::handle_event() POLLNVAL";
  }

  if (m_recv_events & (POLLERR | POLLNVAL)) {
    if (m_error_callback) m_error_callback();
  }
  if (m_recv_events & (POLLIN | POLLPRI | POLLRDHUP)) {
    if (m_read_callback) m_read_callback(recv_time);
  }
  if (m_recv_events & POLLOUT) {
    if (m_write_callback) m_write_callback();
  }
  // WARNING: may not be handled. Just registered.
  // LOG_TRACE << "fd = " << m_fd << " event handled.";
  LOG_TRACE << "fd = " << m_fd << " event handled.";
  m_is_handing_event = false;
}

string Channel::recv_events_to_string() const {
  return events_to_string(m_fd, m_recv_events);
}

string Channel::concerned_events_to_string() const {
  return events_to_string(m_fd, m_concerned_events);
}

string Channel::events_to_string(int fd, int ev) {
  std::ostringstream oss;
  oss << fd << ": ";
  if (ev & POLLIN) oss << "IN ";
  if (ev & POLLPRI) oss << "PRI ";
  if (ev & POLLOUT) oss << "OUT ";
  if (ev & POLLHUP) oss << "HUP ";
  if (ev & POLLRDHUP) oss << "RDHUP ";
  if (ev & POLLERR) oss << "ERR ";
  if (ev & POLLNVAL) oss << "NVAL ";

  return oss.str();
}
}  // namespace flute
