#include <flute/net/poller/PollPoller.h>
#include <flute/common/LogLine.h>
#include <flute/common/types.h>
#include <flute/net/Channel.h>

#include <assert.h>
#include <errno.h>
#include <poll.h>

namespace flute {

PollPoller::PollPoller(Reactor* reactor) : Poller(reactor) {}

PollPoller::~PollPoller() = default;

Timestamp PollPoller::poll(int timeout_ms, ChannelPtrList* activeChannels) {
  // XXX pollfds_ shouldn't change
  int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeout_ms);
  int savedErrno = errno;
  Timestamp now(Timestamp::now());
  if (numEvents > 0) {
    LOG_TRACE << numEvents << " events happened";
    register_active_channels(numEvents, activeChannels);
  } else if (numEvents == 0) {
    LOG_TRACE << timeout_ms << " ms TIMEOUT. No events received.";
  } else {
    if (savedErrno != EINTR) {
      errno = savedErrno;
      LOG_SYSERR << "PollPoller::poll()";
    }
  }
  return now;
}

void PollPoller::register_active_channels(
    int numEvents, ChannelPtrList* activeChannels) const {
  for (PollFdList::const_iterator pfd = pollfds_.begin();
       pfd != pollfds_.end() && numEvents > 0; ++pfd) {
    if (pfd->revents > 0) {
      --numEvents;
      ChannelMap::const_iterator ch = m_channel_map.find(pfd->fd);
      assert(ch != m_channel_map.end());
      Channel* channel = ch->second;
      assert(channel->fd() == pfd->fd);
      channel->set_revents(pfd->revents);
      // pfd->revents = 0;
      activeChannels->push_back(channel);
    }
  }
}

void PollPoller::update_channel(Channel* channel) {
  Poller::assert_in_reactor_thread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->concerned_events();
  if (channel->index() < 0) {
    // a new one, add to pollfds_
    assert(m_channel_map.find(channel->fd()) == m_channel_map.end());
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->concerned_events());
    pfd.revents = 0;
    pollfds_.push_back(pfd);
    int idx = static_cast<int>(pollfds_.size()) - 1;
    channel->set_index(idx);
    m_channel_map[pfd.fd] = channel;
  } else {
    // update existing one
    assert(m_channel_map.find(channel->fd()) != m_channel_map.end());
    assert(m_channel_map[channel->fd()] == channel);
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd() - 1);
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->concerned_events());
    pfd.revents = 0;
    if (channel->is_none_event()) {
      // ignore this pollfd
      pfd.fd = -channel->fd() - 1;
    }
  }
}

void PollPoller::remove_channel(Channel* channel) {
  Poller::assert_in_reactor_thread();
  LOG_TRACE << "fd = " << channel->fd();
  assert(m_channel_map.find(channel->fd()) != m_channel_map.end());
  assert(m_channel_map[channel->fd()] == channel);
  assert(channel->is_none_event());
  int idx = channel->index();
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  const struct pollfd& pfd = pollfds_[idx];
  (void)pfd;
  assert(pfd.fd == -channel->fd() - 1 && pfd.events == channel->concerned_events());
  size_t n = m_channel_map.erase(channel->fd());
  assert(n == 1);
  (void)n;
  if (implicit_cast<size_t>(idx) == pollfds_.size() - 1) {
    pollfds_.pop_back();
  } else {
    int channelAtEnd = pollfds_.back().fd;
    iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);
    if (channelAtEnd < 0) {
      channelAtEnd = -channelAtEnd - 1;
    }
    m_channel_map[channelAtEnd]->set_index(idx);
    pollfds_.pop_back();
  }
}

}  // namespace flute