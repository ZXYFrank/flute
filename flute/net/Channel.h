#ifndef FLUTE_NET_CHANNEL_H
#define FLUTE_NET_CHANNEL_H

#include <flute/common/Timestamp.h>
#include <flute/common/noncopyable.h>

#include <functional>
#include <memory>

#include "poll.h"

namespace flute {

class Reactor;

enum PollEvents {
  kNoneEvent = 0,
  kReadEvent = POLLIN | POLLPRI,
  kWriteEvent = POLLOUT
};

class Channel : noncopyable {
 public:
  typedef std::function<void()> EventCallback;
  typedef std::function<void(Timestamp)> ReadEventCallback;

  Channel(Reactor* reactor, int fd);
  ~Channel();

  void handle_event(Timestamp receiveTime);
  void set_read_callback(ReadEventCallback cb) {
    m_read_callback = std::move(cb);
  }
  void set_write_callback(EventCallback cb) {
    m_write_callback = std::move(cb);
  }
  void set_close_callback(EventCallback cb) {
    m_close_callback = std::move(cb);
  }
  void set_error_callback(EventCallback cb) {
    m_error_callback = std::move(cb);
  }

  void tie(const std::shared_ptr<void>&);

  int fd() const { return m_fd; }
  int concerned_events() const { return m_concerned_events; }
  void set_revents(int revt) { m_recv_events = revt; }  // used by pollers
  // int revents() const { return m_recv_events; }
  bool is_none_event() const { return m_concerned_events == kNoneEvent; }

  void wang_to_read() {
    m_concerned_events |= kReadEvent;
    update_self_in_reactor();
  }
  void end_reading() {
    m_concerned_events &= ~kReadEvent;
    update_self_in_reactor();
  }
  void want_to_write() {
    m_concerned_events |= kWriteEvent;
    update_self_in_reactor();
  }
  void end_writing() {
    m_concerned_events &= ~kWriteEvent;
    update_self_in_reactor();
  }
  void end_all() {
    m_concerned_events = kNoneEvent;
    update_self_in_reactor();
  }
  bool is_writing() const { return m_concerned_events & kWriteEvent; }
  bool is_reading() const { return m_concerned_events & kReadEvent; }

  // for Poller
  int index() { return m_idx; }
  void set_index(int idx) { m_idx = idx; }

  // for debug
  string recv_events_to_string() const;
  string concerned_events_to_string() const;

  void disable_loghup() { m_loghup_enabled = false; }

  Reactor* m_owner_reactor() { return m_reactor; }
  void remove_self_from_reactor();

 private:
  static string events_to_string(int fd, int ev);

  void update_self_in_reactor();
  void handle_event_with_guard(Timestamp receiveTime);

  Reactor* m_reactor;
  const int m_fd;
  int m_concerned_events;
  int m_recv_events;  // it's the received event types of epoll or poll
  int m_idx;          // used by Poller.
  bool m_loghup_enabled;

  std::weak_ptr<void> m_tied_obj_ptr;
  bool m_tied;
  bool m_is_handing_event;
  bool m_is_in_reactor;
  ReadEventCallback m_read_callback;
  EventCallback m_write_callback;
  EventCallback m_close_callback;
  EventCallback m_error_callback;
};

}  // namespace flute

#endif  // FLUTE_NET_CHANNEL_H
