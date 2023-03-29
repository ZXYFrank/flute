#include <assert.h>
#include <errno.h>
#include <flute/common/LogLine.h>
#include <flute/net/Channel.h>
#include <flute/net/poller/EPollPoller.h>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>

namespace flute {

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
static_assert(EPOLLIN == POLLIN, "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI, "epoll uses same flag values as poll");
static_assert(EPOLLOUT == POLLOUT, "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP, "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR, "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP, "epoll uses same flag values as poll");

namespace {
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}  // namespace

EPollPoller::EPollPoller(Reactor* reactor)
    : Poller(reactor),
      m_epoll_fd(::epoll_create1(EPOLL_CLOEXEC)),
      m_events_list(kInitEventListSize) {
  if (m_epoll_fd < 0) {
    LOG_SYSFATAL << "EPollPoller::EPollPoller";
  }
}

EPollPoller::~EPollPoller() { ::close(m_epoll_fd); }

Timestamp EPollPoller::poll(int timeout_ms,
                            ChannelPtrList* active_channels_list) {
  LOG_TRACE << "fd total count " << m_channel_map.size();
  int n_events_arrived =
      ::epoll_wait(m_epoll_fd, &*m_events_list.begin(),
                   static_cast<int>(m_events_list.size()), timeout_ms);
  int saved_errorno = errno;
  Timestamp now(Timestamp::now());
  if (n_events_arrived > 0) {
    LOG_TRACE << n_events_arrived << " events happened";
    register_active_channels(n_events_arrived, active_channels_list);
    if (implicit_cast<size_t>(n_events_arrived) == m_events_list.size()) {
      m_events_list.resize(m_events_list.size() * 2);
    }
  } else if (n_events_arrived == 0) {
    LOG_TRACE << timeout_ms << " ms TIMEOUT. No events received.";
  } else {
    // error happens, log uncommon ones
    if (saved_errorno != EINTR) {
      errno = saved_errorno;
      LOG_SYSERR << "EPollPoller::poll()";
    }
  }
  return now;
}

void EPollPoller::register_active_channels(
    int n_events_arrived, ChannelPtrList* active_channels_list) const {
  assert(implicit_cast<size_t>(n_events_arrived) <= m_events_list.size());
  for (int i = 0; i < n_events_arrived; ++i) {
    Channel* channel = static_cast<Channel*>(m_events_list[i].data.ptr);
#ifndef NDEBUG
    int fd = channel->fd();
    ChannelMap::const_iterator it = m_channel_map.find(fd);
    assert(it != m_channel_map.end());
    assert(it->second == channel);
#endif
    channel->set_revents(m_events_list[i].events);
    active_channels_list->push_back(channel);
  }
}

void EPollPoller::update_channel(Channel* channel) {
  Poller::assert_in_reactor_thread();
  const int index = channel->index();
  LOG_TRACE << "fd = " << channel->fd()
            << " events = " << channel->concerned_events_to_string()
            << " index = " << index;
  if (index == kNew || index == kDeleted) {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd();
    if (index == kNew) {
      assert(m_channel_map.find(fd) == m_channel_map.end());
      m_channel_map[fd] = channel;
    } else  // index == kDeleted
    {
      assert(m_channel_map.find(fd) != m_channel_map.end());
      assert(m_channel_map[fd] == channel);
    }

    channel->set_index(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {
    // update existing one with EPOLL_CTL_MOD/DEL
    int fd = channel->fd();
    (void)fd;
    assert(m_channel_map.find(fd) != m_channel_map.end());
    assert(m_channel_map[fd] == channel);
    assert(index == kAdded);
    if (channel->is_none_event()) {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

void EPollPoller::remove_channel(Channel* channel) {
  Poller::assert_in_reactor_thread();
  int fd = channel->fd();
  LOG_TRACE << "fd = " << fd;
  assert(m_channel_map.find(fd) != m_channel_map.end());
  assert(m_channel_map[fd] == channel);
  assert(channel->is_none_event());
  int index = channel->index();
  assert(index == kAdded || index == kDeleted);
  size_t n = m_channel_map.erase(fd);
  (void)n;
  assert(n == 1);

  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel) {
  struct epoll_event event;
  mem_zero(&event, sizeof event);
  event.events = channel->concerned_events();
  event.data.ptr = channel;
  int fd = channel->fd();
  LOG_TRACE << "epoll_ctl op = " << operationToString(operation)
            << " fd = " << fd << " event = { "
            << channel->concerned_events_to_string() << " }";
  if (::epoll_ctl(m_epoll_fd, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG_SYSERR << "epoll_ctl op =" << operationToString(operation)
                 << " fd =" << fd;
    } else {
      LOG_SYSFATAL << "epoll_ctl op =" << operationToString(operation)
                   << " fd =" << fd;
    }
  }
}

const char* EPollPoller::operationToString(int op) {
  switch (op) {
    case EPOLL_CTL_ADD:
      return "ADD";
    case EPOLL_CTL_DEL:
      return "DEL";
    case EPOLL_CTL_MOD:
      return "MOD";
    default:
      assert(false && "ERROR op");
      return "Unknown Operation";
  }
}

}  // namespace flute
