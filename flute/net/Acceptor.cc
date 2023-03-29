

#include <flute/net/Acceptor.h>

#include <flute/common/LogLine.h>
#include <flute/net/Reactor.h>
#include <flute/net/InetAddress.h>
#include <flute/net/SocketsOps.h>

#include <errno.h>
#include <fcntl.h>
// #include <sys/types.h>
// #include <sys/stat.h>
#include <unistd.h>

using namespace flute;

Acceptor::Acceptor(Reactor* reactor, const InetAddress& listen_addr,
                   bool reuseport)
    : m_reactor(reactor),
      m_accept_sockfd(
          socket_ops::create_sockfd_nonblocking_or_abort(listen_addr.family())),
      m_accept_channel(reactor, m_accept_sockfd.fd()),
      m_is_listening(false),
      m_reserved_fd(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
  assert(m_reserved_fd >= 0);
  m_accept_sockfd.set_reuse_adder(true);
  m_accept_sockfd.set_reuse_port(reuseport);
  m_accept_sockfd.bind_addr(listen_addr);
  m_accept_channel.set_read_callback(
      std::bind(&Acceptor::handle_conn_arrival, this));
}

Acceptor::~Acceptor() {
  m_accept_channel.end_all();
  m_is_listening = false;
  m_accept_channel.remove_self_from_reactor();
  ::close(m_reserved_fd);
}

void Acceptor::listen() {
  m_reactor->assert_in_reactor_thread();
  m_is_listening = true;
  m_accept_sockfd.listen();
  m_accept_channel.wang_to_read();
}

void Acceptor::handle_conn_arrival() {
  m_reactor->assert_in_reactor_thread();
  InetAddress peer_addr;
  // FIXME loop until no more
  int connfd = m_accept_sockfd.accept(&peer_addr);
  if (connfd >= 0) {
    // string hostport = peer_addr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (m_new_conn_callback) {
      m_new_conn_callback(connfd, peer_addr);
    } else {
      socket_ops::close(connfd);
    }
  } else {
    LOG_SYSERR << "in Acceptor::handleRead";
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of libev.

    // EMFILE, too many open files. We use the reserved fd to accept the socket
    // and close it, to notify the client.
    if (errno == EMFILE) {
      ::close(m_reserved_fd);
      m_reserved_fd = ::accept(m_accept_sockfd.fd(), NULL, NULL);
      ::close(m_reserved_fd);
      m_reserved_fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}
