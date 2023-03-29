#ifndef FLUTE_NET_CONNECTOR_H
#define FLUTE_NET_CONNECTOR_H

#include <flute/common/noncopyable.h>
#include <flute/net/InetAddress.h>

#include <functional>
#include <memory>

namespace flute {

class Channel;
class Reactor;

class Connector : noncopyable, public std::enable_shared_from_this<Connector> {
 public:
  typedef std::function<void(int sockfd)> NewConnectionCallback;

  Connector(Reactor* reactor, const InetAddress& serverAddr);
  ~Connector();

  void set_new_conn_callback(const NewConnectionCallback& cb) {
    m_new_conn_callback = cb;
  }

  void start();    // can be called in any thread
  void restart();  // must be called in loop thread
  void stop();     // can be called in any thread

  const InetAddress& serverAddress() const { return serverAddr_; }

 private:
  enum ConnectStates { kDisconnected, kConnecting, kConnected };
  static const int kMaxRetryDelayMs = 30 * 1000;
  static const int kInitRetryDelayMs = 500;

  void set_state(ConnectStates s) { m_state = s; }
  void start_in_reactor();
  void stop_in_reactor();
  void connect();
  void connecting(int sockfd);
  void handle_write();
  void handle_error();
  void retry(int sockfd);
  int remove_and_reset_channel();
  void reset_channel();

  Reactor* m_reactor;
  InetAddress serverAddr_;
  bool m_is_connected;    // atomic
  ConnectStates m_state;  // FIXME: use atomic variable
  std::unique_ptr<Channel> m_channel;
  NewConnectionCallback m_new_conn_callback;
  int m_retry_delay_ms;
};

}  // namespace flute

#endif  // FLUTE_NET_CONNECTOR_H
