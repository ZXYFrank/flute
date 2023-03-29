#ifndef FLUTE_NET_POLLER_EPOLLPOLLER_H
#define FLUTE_NET_POLLER_EPOLLPOLLER_H

#include <flute/net/Poller.h>

#include <vector>

struct epoll_event;

namespace flute {

///
/// IO Multiplexing with epoll(4).
///
class EPollPoller : public Poller {
 public:
  EPollPoller(Reactor* reactor);
  ~EPollPoller() override;

  Timestamp poll(int timeout_ms, ChannelPtrList* active_channels) override;
  void update_channel(Channel* channel) override;
  void remove_channel(Channel* channel) override;

 private:
  static const int kInitEventListSize = 16;

  static const char* operationToString(int op);

  void register_active_channels(int numEvents,
                            ChannelPtrList* active_channels) const;
  void update(int operation, Channel* channel);

  typedef std::vector<struct epoll_event> EventList;

  int m_epoll_fd;
  EventList m_events_list;
};

}  // namespace flute
#endif  // FLUTE_NET_POLLER_EPOLLPOLLER_H
