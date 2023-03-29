#include <flute/net/Poller.h>
#include <flute/net/poller/PollPoller.h>
#include <flute/net/poller/EPollPoller.h>

#include <stdlib.h>

namespace flute {

Poller* Poller::new_default_poller(Reactor* reactor) {
  if (::getenv("FLUTE_USE_POLL")) {
    return new PollPoller(reactor);
  } else {
    return new EPollPoller(reactor);
  }
}

}  // namespace flute