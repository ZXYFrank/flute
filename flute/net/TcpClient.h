
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_TCPCLIENT_H
#define FLUTE_NET_TCPCLIENT_H

#include <flute/common/Mutex.h>
#include <flute/net/TcpConnection.h>

namespace flute {

class Connector;
typedef std::shared_ptr<Connector> ConnectorPtr;

class TcpClient : noncopyable {
 public:
  // TcpClient(Reactor* reactor);
  // TcpClient(Reactor* reactor, const string& host, uint16_t port);
  TcpClient(Reactor* reactor, const InetAddress& serverAddr,
            const string& nameArg);
  ~TcpClient();  // force out-line dtor, for std::unique_ptr members.

  void connect();
  void disconnect();
  void stop();

  TcpConnectionPtr connection() const {
    MutexLockGuard lock(m_mutexlock);
    return connection_;
  }

  Reactor* get_reactor() const { return m_reactor; }
  bool retry() const { return m_enable_retry; }
  void enable_retry() { m_enable_retry = true; }

  const string& name() const { return m_name; }

  /// Set connection callback.
  /// Not thread safe.
  void set_conn_callback(ConnectionCallback cb) {
    m_conn_callback = std::move(cb);
  }

  /// Set message callback.
  /// Not thread safe.
  void set_msg_callback(MessageCallback cb) { m_msg_callback = std::move(cb); }

  /// Set write complete callback.
  /// Not thread safe.
  void set_write_complete_callback(WriteCompleteCallback cb) {
    m_write_complete_callback = std::move(cb);
  }

 private:
  /// Not thread safe, but in loop
  void new_conn_callback(int sockfd);
  /// Not thread safe, but in loop
  void remove_conn(const TcpConnectionPtr& conn);

  Reactor* m_reactor;
  ConnectorPtr connector_;  // avoid revealing Connector
  const string m_name;
  ConnectionCallback m_conn_callback;
  MessageCallback m_msg_callback;
  WriteCompleteCallback m_write_complete_callback;
  bool m_enable_retry;  // atomic
  bool m_is_connected;  // atomic
  // always in loop thread
  int g_conn_counter;
  mutable MutexLock m_mutexlock;
  TcpConnectionPtr connection_ GUARDED_BY(m_mutexlock);
};

}  // namespace flute

#endif  // FLUTE_NET_TCPCLIENT_H
