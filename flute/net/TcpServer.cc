

#include <flute/net/TcpServer.h>

#include <flute/common/LogLine.h>
#include <flute/net/Acceptor.h>
#include <flute/net/Reactor.h>
#include <flute/net/ReactorThreadPool.h>
#include <flute/net/SocketsOps.h>

#include <stdio.h>  // snprintf

namespace flute {

TcpServer::TcpServer(Reactor* reactor, const InetAddress& listen_addr,
                     const string& name_arg, Option option)
    : m_acceptor_reactor(CHECK_NOTNULL(reactor)),
      m_ip_port(listen_addr.to_ip_port()),
      m_name(name_arg),
      m_acceptor(new Acceptor(reactor, listen_addr, option == kReusePort)),
      m_reactor_thread_poll(new ReactorThreadPool(reactor, m_name)),
      m_conn_callback(dummy_conn_callback),
      m_message_callback(dummy_message_callback) {
  g_conn_counter.get_and_set(0);
  m_acceptor->set_new_conn_callback(
      std::bind(&TcpServer::new_conn_callback, this, _1, _2));
}

TcpServer::~TcpServer() {
  m_acceptor_reactor->assert_in_reactor_thread();
  LOG_TRACE << "TcpServer::~TcpServer [" << m_name << "] destructing";

  for (auto& item : m_conn_map) {
    TcpConnectionPtr conn(item.second);
    item.second.reset();
    conn->get_reactor()->run_asap_in_reactor(
        std::bind(&TcpConnection::connect_destroyed, conn));
  }
}

void TcpServer::set_reactor_pool_size(int num_threads) {
  assert(0 <= num_threads);
  m_reactor_thread_poll->set_pool_size(num_threads);
}

void TcpServer::start() {
  if (m_has_started.get_and_set(1) == 0) {
    m_reactor_thread_poll->start(m_reactor_thread_init_func);
    assert(!m_acceptor->is_listenning());
    // m_acceptor_reactor starts listen at main port
    m_acceptor_reactor->run_asap_in_reactor(
        std::bind(&Acceptor::listen, get_pointer(m_acceptor)));
  }
}

void TcpServer::new_conn_callback(int sockfd, const InetAddress& peer_addr) {
  m_acceptor_reactor->assert_in_reactor_thread();
  // select a reactor from the pool to be responsible for this sockfd.
  Reactor* sockfd_reactor = m_reactor_thread_poll->get_next_reactor();
  char buf[64];
  snprintf(buf, sizeof buf, ":%s#%d", m_ip_port.c_str(), g_conn_counter.get());
  g_conn_counter.add_and_get(1);
  string conn_name = m_name + buf;

  LOG_INFO << "TcpServer::new_conn_callback [" << m_name
           << "] - new connection [" << conn_name << "] from "
           << peer_addr.to_ip_port();
  InetAddress local_addr(socket_ops::get_local_addr(sockfd));

  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  TcpConnectionPtr tcp_conn(new TcpConnection(sockfd_reactor, conn_name, sockfd,
                                              local_addr, peer_addr));
  m_conn_map[conn_name] = tcp_conn;
  tcp_conn->set_conn_callback(m_conn_callback);
  tcp_conn->set_message_callback(m_message_callback);
  // WARNING: not set
  tcp_conn->set_write_complete_callback(m_write_complete_callback);
  // ONGOING: weak ptr
  tcp_conn->set_close_callback(
      std::bind(&TcpServer::remove_conn, this, _1));  // FIXME: unsafe
  sockfd_reactor->run_asap_in_reactor(
      std::bind(&TcpConnection::connect_established, tcp_conn));
}

void TcpServer::remove_conn(const TcpConnectionPtr& conn) {
  // FIXME: unsafe
  m_acceptor_reactor->run_asap_in_reactor(
      std::bind(&TcpServer::remove_connection_in_reactor, this, conn));
}

void TcpServer::remove_connection_in_reactor(const TcpConnectionPtr& conn) {
  m_acceptor_reactor->assert_in_reactor_thread();
  LOG_INFO << "TcpServer::remove_connection_in_reactor [" << m_name
           << "] - connection " << conn->name();
  size_t n = m_conn_map.erase(conn->name());
  (void)n;
  assert(n == 1);
  Reactor* sockfd_reactor = conn->get_reactor();
  sockfd_reactor->queue_in_reactor(
      std::bind(&TcpConnection::connect_destroyed, conn));
}

}  // namespace flute