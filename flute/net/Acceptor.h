#ifndef FLUTE_NET_ACCEPTOR_H
#define FLUTE_NET_ACCEPTOR_H

#include <functional>

#include <flute/net/Channel.h>
#include <flute/net/Socket.h>

namespace flute {

class Reactor;
class InetAddress;

///
/// Acceptor of incoming TCP connections.
///
class Acceptor : noncopyable {
 public:
  typedef std::function<void(int sockfd, const InetAddress&)>
      NewConnectionCallback;

  Acceptor(Reactor* reactor, const InetAddress& listenAddr, bool reuseport);
  ~Acceptor();

  void set_new_conn_callback(const NewConnectionCallback& cb) {
    m_new_conn_callback = cb;
  }

  bool is_listenning() const { return m_is_listening; }
  void listen();

 private:
  void handle_conn_arrival();

  Reactor* m_reactor;
  Socket m_accept_sockfd;
  Channel m_accept_channel;
  NewConnectionCallback m_new_conn_callback;
  bool m_is_listening;
  int m_reserved_fd;
};

}  // namespace flute

#endif  // FLUTE_NET_ACCEPTOR_H
