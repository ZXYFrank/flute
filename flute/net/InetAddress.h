
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_INETADDRESS_H
#define FLUTE_NET_INETADDRESS_H

#include <flute/common/StringPiece.h>

#include <netinet/in.h>

namespace flute {

namespace socket_ops {
const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
}

///
/// Wrapper of sockaddr_in.
///
/// This is an POD interface class.
class InetAddress {
 public:
  /// Constructs an endpoint with given port number.
  /// Mostly used in TcpServer listening.
  explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false,
                       bool ipv6 = false);

  /// Constructs an endpoint with given ip and port.
  /// @c ip should be "1.2.3.4"
  InetAddress(StringArg ip, uint16_t port, bool ipv6 = false);

  /// Constructs an endpoint with given struct @c sockaddr_in
  /// Mostly used when accepting new connections
  explicit InetAddress(const struct sockaddr_in& addr) : m_addr(addr) {}

  explicit InetAddress(const struct sockaddr_in6& addr) : m_addr6(addr) {}

  sa_family_t family() const { return m_addr.sin_family; }
  string to_ip() const;
  string to_ip_port() const;
  uint16_t to_port() const;

  // default copy/assignment are Okay

  const struct sockaddr* get_sock_addr() const {
    return socket_ops::sockaddr_cast(&m_addr6);
  }
  void set_sock_addr_inet6(const struct sockaddr_in6& addr6) {
    m_addr6 = addr6;
  }

  uint32_t ip_net_endian() const;
  uint16_t port_net_endian() const { return m_addr.sin_port; }

  // resolve hostname to IP address, not changing port or sin_family
  // return true on success.
  // thread safe
  static bool resolve(StringArg hostname, InetAddress* result);
  // static std::vector<InetAddress> resolveAll(const char* hostname, uint16_t
  // port = 0);

 private:
  union {
    struct sockaddr_in m_addr;
    struct sockaddr_in6 m_addr6;
  };
};

}  // namespace flute

#endif  // FLUTE_NET_INETADDRESS_H
