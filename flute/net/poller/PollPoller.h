#ifndef FLUTE_NET_POLLER_POLLPOLLER_H
#define FLUTE_NET_POLLER_POLLPOLLER_H

#include <flute/net/Poller.h>

#include <vector>

struct pollfd;

namespace flute {

///
/// IO Multiplexing with poll(2).
///
class PollPoller : public Poller {
 public:
  PollPoller(Reactor* reactor);
  ~PollPoller() override;

  Timestamp poll(int timeoutMs, ChannelPtrList* activeChannels) override;
  void update_channel(Channel* channel) override;
  void remove_channel(Channel* channel) override;

 private:
  void register_active_channels(int numEvents, ChannelPtrList* activeChannels) const;

  typedef std::vector<struct pollfd> PollFdList;
  PollFdList pollfds_;
};

}  // namespace flute
#endif  // FLUTE_NET_POLLER_POLLPOLLER_H
