

#include <flute/net/Socket.h>

#include <flute/common/LogLine.h>
#include <flute/net/InetAddress.h>
#include <flute/net/SocketsOps.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>  // snprintf

namespace flute {

Socket::~Socket() {
  socket_ops::close(m_sockfd);
  LOG_TRACE << "Socket instance fd=" << m_sockfd << "deconstrcuted.";
}

bool Socket::get_tcp_info(struct tcp_info* tcpi) const {
  socklen_t len = sizeof(*tcpi);
  mem_zero(tcpi, len);
  return ::getsockopt(m_sockfd, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::get_tcp_info_string(char* buf, int len) const {
  struct tcp_info tcpi;
  bool ok = get_tcp_info(&tcpi);
  if (ok) {
    snprintf(
        buf, len,
        "unrecovered=%u "
        "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
        "lost=%u retrans=%u rtt=%u rttvar=%u "
        "sshthresh=%u cwnd=%u total_retrans=%u",
        tcpi.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
        tcpi.tcpi_rto,          // Retransmit timeout in usec
        tcpi.tcpi_ato,          // Predicted tick of soft clock in usec
        tcpi.tcpi_snd_mss, tcpi.tcpi_rcv_mss,
        tcpi.tcpi_lost,     // Lost packets
        tcpi.tcpi_retrans,  // Retransmitted packets out
        tcpi.tcpi_rtt,      // Smoothed round trip time in usec
        tcpi.tcpi_rttvar,   // Medium deviation
        tcpi.tcpi_snd_ssthresh, tcpi.tcpi_snd_cwnd,
        tcpi.tcpi_total_retrans);  // Total retransmits for entire connection
  }
  return ok;
}

void Socket::bind_addr(const InetAddress& addr) {
  socket_ops::bind_or_abort(m_sockfd, addr.get_sock_addr());
}

void Socket::listen() { socket_ops::listen_or_abort(m_sockfd); }

int Socket::accept(InetAddress* peeraddr) {
  struct sockaddr_in6 addr;
  mem_zero(&addr, sizeof addr);
  int connfd = socket_ops::accept(m_sockfd, &addr);
  if (connfd >= 0) {
    peeraddr->set_sock_addr_inet6(addr);
  } else {
    LOG_FATAL << "Accept Failure: Socket::accept got error" << errno;
  }
  return connfd;
}

void Socket::shutdown_write() { socket_ops::shutdown_write(m_sockfd); }

void Socket::set_tcp_nodelay(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(m_sockfd, IPPROTO_TCP, TCP_NODELAY, &optval,
                         static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    LOG_SYSERR << "set_reuse_adder failed.";
  }
}

void Socket::set_reuse_adder(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval,
                         static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    LOG_SYSERR << "set_reuse_adder failed.";
  }
}

void Socket::set_reuse_port(bool on) {
#ifdef SO_REUSEPORT
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &optval,
                         static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    LOG_SYSERR << "SO_REUSEPORT failed.";
  }
#else
  if (on) {
    LOG_ERROR << "SO_REUSEPORT is not supported.";
  }
#endif
}

void Socket::set_keep_alive(bool on) {
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(m_sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval,
                         static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    LOG_SYSERR << "set_reuse_adder failed.";
  }
}

}  // namespace flute
