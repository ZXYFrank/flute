

#include <flute/net/Poller.h>

#include <flute/net/Channel.h>

using namespace flute;

Poller::Poller(Reactor* reactor) : m_owner_reactor_(reactor) {}

Poller::~Poller() = default;

bool Poller::has_channel(Channel* channel) const {
  assert_in_reactor_thread();
  ChannelMap::const_iterator it = m_channel_map.find(channel->fd());
  return it != m_channel_map.end() && it->second == channel;
}
