

#include <flute/net/InetAddress.h>

#include <flute/common/LogLine.h>
#include <flute/net/Endian.h>
#include <flute/net/SocketsOps.h>

#include <netdb.h>
#include <netinet/in.h>

// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };

using namespace flute;

static_assert(sizeof(InetAddress) == sizeof(struct sockaddr_in6),
              "InetAddress is same size as sockaddr_in6");
static_assert(offsetof(sockaddr_in, sin_family) == 0, "sin_family offset 0");
static_assert(offsetof(sockaddr_in6, sin6_family) == 0, "sin6_family offset 0");
static_assert(offsetof(sockaddr_in, sin_port) == 2, "sin_port offset 2");
static_assert(offsetof(sockaddr_in6, sin6_port) == 2, "sin6_port offset 2");

InetAddress::InetAddress(uint16_t port, bool loopbackOnly, bool ipv6) {
  static_assert(offsetof(InetAddress, m_addr6) == 0, "m_addr6 offset 0");
  static_assert(offsetof(InetAddress, m_addr) == 0, "m_addr offset 0");
  if (ipv6) {
    mem_zero(&m_addr6, sizeof m_addr6);
    m_addr6.sin6_family = AF_INET6;
    in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
    m_addr6.sin6_addr = ip;
    m_addr6.sin6_port = socket_ops::host_to_net16(port);
  } else {
    mem_zero(&m_addr, sizeof m_addr);
    m_addr.sin_family = AF_INET;
    in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
    m_addr.sin_addr.s_addr = socket_ops::host_to_net32(ip);
    m_addr.sin_port = socket_ops::host_to_net16(port);
  }
}

InetAddress::InetAddress(StringArg ip, uint16_t port, bool ipv6) {
  if (ipv6) {
    mem_zero(&m_addr6, sizeof m_addr6);
    socket_ops::fromIpPort(ip.c_str(), port, &m_addr6);
  } else {
    mem_zero(&m_addr, sizeof m_addr);
    socket_ops::fromIpPort(ip.c_str(), port, &m_addr);
  }
}

string InetAddress::to_ip_port() const {
  char buf[64] = "";
  socket_ops::to_ip_port(buf, sizeof buf, get_sock_addr());
  return buf;
}

string InetAddress::to_ip() const {
  char buf[64] = "";
  socket_ops::to_ip(buf, sizeof buf, get_sock_addr());
  return buf;
}

uint32_t InetAddress::ip_net_endian() const {
  assert(family() == AF_INET);
  return m_addr.sin_addr.s_addr;
}

uint16_t InetAddress::to_port() const {
  return socket_ops::net_to_host16(port_net_endian());
}

static __thread char t_resolveBuffer[64 * 1024];

bool InetAddress::resolve(StringArg hostname, InetAddress* out) {
  assert(out != NULL);
  struct hostent hent;
  struct hostent* he = NULL;
  int herrno = 0;
  mem_zero(&hent, sizeof(hent));

  int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer,
                            sizeof t_resolveBuffer, &he, &herrno);
  if (ret == 0 && he != NULL) {
    assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
    out->m_addr.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
    return true;
  } else {
    if (ret) {
      LOG_SYSERR << "InetAddress::resolve";
    }
    return false;
  }
}
