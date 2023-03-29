#ifndef FLUTE_NET_POLLER_H
#define FLUTE_NET_POLLER_H

#include <map>
#include <vector>

#include <flute/common/Timestamp.h>
#include <flute/net/Reactor.h>

namespace flute {

class Channel;

///
/// Base class for IO Multiplexing. It will monitor the IO events on multiple
/// Channels.
///
/// This class doesn't own the Channel objects.
class Poller : noncopyable {
 public:
  typedef std::vector<Channel*> ChannelPtrList;

  Poller(Reactor* reactor);
  virtual ~Poller();

  /// Polls the I/O events.
  /// Must be called in the loop thread.
  virtual Timestamp poll(int timeout_ms, ChannelPtrList* ready_channels1) = 0;

  /// Changes the interested I/O events.
  /// Must be called in the loop thread.
  virtual void update_channel(Channel* channel) = 0;

  /// Remove the channel, when it destructs.
  /// Must be called in the loop thread.
  virtual void remove_channel(Channel* channel) = 0;

  virtual bool has_channel(Channel* channel) const;

  static Poller* new_default_poller(Reactor* reactor);

  void assert_in_reactor_thread() const {
    m_owner_reactor_->assert_in_reactor_thread();
  }

 protected:
  typedef std::map<int, Channel*> ChannelMap;
  ChannelMap m_channel_map;

 private:
  Reactor* m_owner_reactor_;
};

}  // namespace flute

#endif  // FLUTE_NET_POLLER_H
