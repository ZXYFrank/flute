
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_TCPSERVER_H
#define FLUTE_NET_TCPSERVER_H

#include <flute/common/Atomic.h>
#include <flute/common/types.h>
#include <flute/net/TcpConnection.h>

#include <map>

namespace flute {

class Acceptor;
class Reactor;
class ReactorThreadPool;

///
/// TCP server, supports single-threaded and thread-pool models.
///
/// This is an interface class, so don't expose too much details.
class TcpServer : noncopyable {
 public:
  typedef std::function<void(Reactor*)> ThreadInitFunctor;
  enum Option {
    kNoReusePort,
    kReusePort,
  };

  // TcpServer(Reactor* reactor, const InetAddress& listenAddr);
  // QUESTION: why not create a reactor by itself?
  TcpServer(Reactor* reactor, const InetAddress& listenAddr,
            const string& nameArg, Option option = kNoReusePort);
  ~TcpServer();  // force out-line dtor, for std::unique_ptr members.

  const string& ip_port() const { return m_ip_port; }
  const string& name() const { return m_name; }
  Reactor* get_listen_reactor() const { return m_acceptor_reactor; }

  /// Set the number of threads for handling input.
  ///
  /// Always accepts new connection in loop's thread.
  /// Must be called before @c start
  /// @param numThreads
  /// - 0 means all I/O in loop's thread, no thread will created.
  ///   this is the default value.
  /// - 1 means all I/O in another thread.
  /// - N means a thread pool with N threads, new connections
  ///   are assigned on a round-robin basis.
  void set_reactor_pool_size(int numThreads);
  void set_reactor_init_func(const ThreadInitFunctor& cb) {
    m_reactor_thread_init_func = cb;
  }
  /// valid after calling start()
  std::shared_ptr<ReactorThreadPool> threadPool() {
    return m_reactor_thread_poll;
  }

  /// Starts the server if it's not listenning.
  ///
  /// It's harmless to call it multiple times.
  /// Thread safe.
  void start();

  /// Set connection callback.
  /// Not thread safe.
  void set_conn_callback(const ConnectionCallback& cb) { m_conn_callback = cb; }

  /// Set Message callback.
  /// Not thread safe.
  void set_message_callback(const MessageCallback& cb) {
    m_message_callback = cb;
  }

  /// Set write complete callback.
  /// Not thread safe.
  void set_write_complete_callback(const WriteCompleteCallback& cb) {
    m_write_complete_callback = cb;
  }

 private:
  /// Not thread safe, but in loop
  void new_conn_callback(int sockfd, const InetAddress& peer_addr);
  /// Thread safe.
  void remove_conn(const TcpConnectionPtr& conn);
  /// Not thread safe, but in loop
  void remove_connection_in_reactor(const TcpConnectionPtr& conn);

  typedef std::map<string, TcpConnectionPtr> ConnectionMap;

  Reactor* m_acceptor_reactor;  // the acceptor loop
  const string m_ip_port;
  const string m_name;
  std::unique_ptr<Acceptor> m_acceptor;  // avoid revealing Acceptor
  std::shared_ptr<ReactorThreadPool> m_reactor_thread_poll;
  ConnectionCallback m_conn_callback;
  MessageCallback m_message_callback;
  WriteCompleteCallback m_write_complete_callback;
  ThreadInitFunctor m_reactor_thread_init_func;
  AtomicInt32 m_has_started;
  // always in loop thread
  AtomicInt32 g_conn_counter;
  // Store all the TCP Connections this TCP Server established.
  ConnectionMap m_conn_map;
};

}  // namespace flute

#endif  // FLUTE_NET_TCPSERVER_H
