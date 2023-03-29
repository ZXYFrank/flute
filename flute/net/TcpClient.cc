
//

#include <flute/net/TcpClient.h>

#include <flute/common/LogLine.h>
#include <flute/net/Connector.h>
#include <flute/net/Reactor.h>
#include <flute/net/SocketsOps.h>

#include <stdio.h>  // snprintf

using namespace flute;


// TcpClient::TcpClient(Reactor* reactor)
//   : m_reactor(reactor)
// {
// }

// TcpClient::TcpClient(Reactor* reactor, const string& host, uint16_t port)
//   : m_reactor(CHECK_NOTNULL(reactor)),
//     serverAddr_(host, port)
// {
// }

namespace flute
{

namespace detail
{

void remove_conn(Reactor* reactor, const TcpConnectionPtr& conn)
{
  reactor->queue_in_reactor(std::bind(&TcpConnection::connect_destroyed, conn));
}

void removeConnector(const ConnectorPtr& connector)
{
  //connector->
}

}  // namespace detail

}  // namespace flute

TcpClient::TcpClient(Reactor* reactor,
                     const InetAddress& serverAddr,
                     const string& nameArg)
  : m_reactor(CHECK_NOTNULL(reactor)),
    connector_(new Connector(reactor, serverAddr)),
    m_name(nameArg),
    m_conn_callback(dummy_conn_callback),
    m_msg_callback(dummy_message_callback),
    m_enable_retry(false),
    m_is_connected(true),
    g_conn_counter(1)
{
  connector_->set_new_conn_callback(
      std::bind(&TcpClient::new_conn_callback, this, _1));
  // FIXME setConnectFailedCallback
  LOG_INFO << "TcpClient::TcpClient[" << m_name
           << "] - connector " << get_pointer(connector_);
}

TcpClient::~TcpClient()
{
  LOG_INFO << "TcpClient::~TcpClient[" << m_name
           << "] - connector " << get_pointer(connector_);
  TcpConnectionPtr conn;
  bool unique = false;
  {
    MutexLockGuard lock(m_mutexlock);
    unique = connection_.unique();
    conn = connection_;
  }
  if (conn)
  {
    assert(m_reactor == conn->get_reactor());
    // FIXME: not 100% safe, if we are in different thread
    CloseCallback cb = std::bind(&detail::remove_conn, m_reactor, _1);
    m_reactor->run_asap_in_reactor(
        std::bind(&TcpConnection::set_close_callback, conn, cb));
    if (unique)
    {
      conn->force_close();
    }
  }
  else
  {
    connector_->stop();
    // FIXME: HACK
    m_reactor->run_after(1, std::bind(&detail::removeConnector, connector_));
  }
}

void TcpClient::connect()
{
  // FIXME: check state
  LOG_INFO << "TcpClient::connect[" << m_name << "] - connecting to "
           << connector_->serverAddress().to_ip_port();
  m_is_connected = true;
  connector_->start();
}

void TcpClient::disconnect()
{
  m_is_connected = false;

  {
    MutexLockGuard lock(m_mutexlock);
    if (connection_)
    {
      connection_->shutdown();
    }
  }
}

void TcpClient::stop()
{
  m_is_connected = false;
  connector_->stop();
}

void TcpClient::new_conn_callback(int sockfd)
{
  m_reactor->assert_in_reactor_thread();
  InetAddress peer_addr(socket_ops::get_peer_addr(sockfd));
  char buf[32];
  snprintf(buf, sizeof buf, ":%s#%d", peer_addr.to_ip_port().c_str(), g_conn_counter);
  ++g_conn_counter;
  string connName = m_name + buf;

  InetAddress local_addr(socket_ops::get_local_addr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  TcpConnectionPtr conn(new TcpConnection(m_reactor,
                                          connName,
                                          sockfd,
                                          local_addr,
                                          peer_addr));

  conn->set_conn_callback(m_conn_callback);
  conn->set_message_callback(m_msg_callback);
  conn->set_write_complete_callback(m_write_complete_callback);
  conn->set_close_callback(
      std::bind(&TcpClient::remove_conn, this, _1)); // FIXME: unsafe
  {
    MutexLockGuard lock(m_mutexlock);
    connection_ = conn;
  }
  conn->connect_established();
}

void TcpClient::remove_conn(const TcpConnectionPtr& conn)
{
  m_reactor->assert_in_reactor_thread();
  assert(m_reactor == conn->get_reactor());

  {
    MutexLockGuard lock(m_mutexlock);
    assert(connection_ == conn);
    connection_.reset();
  }

  m_reactor->queue_in_reactor(std::bind(&TcpConnection::connect_destroyed, conn));
  if (m_enable_retry && m_is_connected)
  {
    LOG_INFO << "TcpClient::connect[" << m_name << "] - Reconnecting to "
             << connector_->serverAddress().to_ip_port();
    connector_->restart();
  }
}

