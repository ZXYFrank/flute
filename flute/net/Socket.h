#ifndef FLUTE_NET_SOCKET_H
#define FLUTE_NET_SOCKET_H

#include <flute/common/noncopyable.h>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace flute {
///
/// TCP networking.
///

class InetAddress;

///
/// Wrapper of socket file descriptor.
///
/// It closes the sockfd when desctructs.
/// It's thread safe, all operations are delagated to OS.
class Socket : noncopyable {
 public:
  explicit Socket(int sockfd) : m_sockfd(sockfd) {}

  explicit Socket(const Socket& socket) : m_sockfd(socket.fd()){};
  explicit Socket(Socket&& socket) : m_sockfd(socket.fd()){};

  ~Socket();

  int fd() const { return m_sockfd; }
  // return true if success.
  bool get_tcp_info(struct tcp_info*) const;
  bool get_tcp_info_string(char* buf, int len) const;

  /// abort if address in use
  void bind_addr(const InetAddress& localaddr);
  /// abort if address in use
  void listen();

  /// On success, returns a non-negative integer that is
  /// a descriptor for the accepted socket, which has been
  /// set to non-blocking and close-on-exec. *peeraddr is assigned.
  /// On error, -1 is returned, and *peeraddr is untouched.
  int accept(InetAddress* peeraddr);

  void shutdown_write();

  ///
  /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
  ///
  void set_tcp_nodelay(bool on);

  ///
  /// Enable/disable SO_REUSEADDR
  ///
  void set_reuse_adder(bool on);

  ///
  /// Enable/disable SO_REUSEPORT
  ///
  void set_reuse_port(bool on);

  ///
  /// Enable/disable SO_KEEPALIVE
  ///
  void set_keep_alive(bool on);

 private:
  const int m_sockfd;
};

}  // namespace flute

#endif  // FLUTE_NET_SOCKET_H
