
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_TCPCONNECTION_H
#define FLUTE_NET_TCPCONNECTION_H

#include <flute/common/StringPiece.h>
#include <flute/common/ZeroCopier.h>
#include <flute/common/noncopyable.h>
#include <flute/common/types.h>
#include <flute/net/Buffer.h>
#include <flute/net/Callbacks.h>
#include <flute/net/InetAddress.h>

#include <any>
#include <memory>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace flute {

class Channel;
class Reactor;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
 public:
  /// Constructs a TcpConnection with a connected sockfd
  ///
  /// User should not create this object.
  TcpConnection(Reactor* reactor, const string& name, int sockfd,
                const InetAddress& local_addr, const InetAddress& peer_addr);
  ~TcpConnection();

  Reactor* get_reactor() const { return m_reactor; }
  const string& name() const { return m_name; }
  const InetAddress& local_address() const { return m_local_addr; }
  const InetAddress& peer_address() const { return m_peer_addr; }
  bool connected() const { return m_conn_state == kConnected; }
  bool disconnected() const { return m_conn_state == kDisconnected; }
  // return true if success.
  bool get_tcp_info(struct tcp_info*) const;
  string get_tcp_info_string() const;

  // void send(string&& message); // C++11
  void send(const void* message, int len);
  void send_string_piece(const StringPiece& message);
  // void send(Buffer&& message); // C++11
  void send_buffer(Buffer* message);  // this one will swap data

  void enable_zero_copy(const ZeroCopierPtr& zero_copier_ptr);

  void shutdown();  // NOT thread safe, no simultaneous calling
  void shutdown_and_force_close_after(
      double seconds);  // NOT thread safe, no simultaneous calling
  void force_close();
  void force_close_with_delay(double seconds);
  void set_tcp_nodelay(bool on);
  // reading or not
  void start_read();
  void stop_read();
  bool is_reading() const {
    return m_is_reading;
  };  // NOT thread safe, may race with start/stop_read_in_reactor

  void set_context_ptr(void* context) { m_context_ptr = context; }

  const void* get_context_ptr() const { return m_context_ptr; }

  void* get_mutable_context_ptr() { return m_context_ptr; }

  void set_conn_callback(const ConnectionCallback& cb) { m_conn_callback = cb; }

  void set_message_callback(const MessageCallback& cb) {
    m_message_callback = cb;
  }

  void set_write_complete_callback(const WriteCompleteCallback& cb) {
    m_write_complete_callback = cb;
  }

  void set_highwater_mark_callback(const HighWaterMarkCallback& cb,
                                   size_t highWaterMark) {
    m_high_watermark_callback = cb;
    m_highwater_mark = highWaterMark;
  }

  /// Advanced interface
  Buffer* input_buffer() { return &m_input_buffer; }

  Buffer* output_buffer() { return &m_output_buffer; }

  /// Internal use only.
  void set_close_callback(const CloseCallback& cb) { m_close_callback = cb; }

  // called when TcpServer accepts a new connection
  void connect_established();  // should be called only once
  // called when TcpServer has removed me from its map
  void connect_destroyed();  // should be called only once

  Channel* get_channel_ptr() { return m_channel.get(); }

 private:
  enum TCPConnectionState {
    kDisconnected,
    kConnecting,
    kConnected,
    kDisconnecting
  };
  enum TCPSendingState { kSendingBuffer, kSendingFile, kNotSending,kSendingFinished };
  TCPSendingState m_sending_state;
  void transfer_sending_state();
  void handle_socket_writable();
  void handle_close();
  void handle_error();
  void write_socket_from_buffer();
  void handle_socket_readable(Timestamp receiveTime);
  // void send_in_reactor(string&& message);
  void send_in_reactor(const StringPiece& message);
  void send_bytes_in_reactor(const void* message, size_t len);
  void shutdown_in_reactor();
  void shutdown_and_force_close_in_reactor_after(double seconds);
  void force_close_in_reactor();
  void set_state(TCPConnectionState s) { m_conn_state = s; }
  const char* state_to_string() const;
  void start_read_in_reactor();
  void stop_read_in_reactor();

  Reactor* m_reactor;
  const string m_name;
  TCPConnectionState m_conn_state;  // FIXME: use atomic variable
  bool m_is_reading;
  // unique means that the fd is totally taken care of by this connection.
  std::unique_ptr<Socket> m_socket;
  std::unique_ptr<Channel> m_channel;
  const InetAddress m_local_addr;
  const InetAddress m_peer_addr;
  ConnectionCallback m_conn_callback;
  MessageCallback m_message_callback;
  WriteCompleteCallback m_write_complete_callback;
  HighWaterMarkCallback m_high_watermark_callback;
  CloseCallback m_close_callback;
  size_t m_highwater_mark;
  Buffer m_input_buffer;
  Buffer m_output_buffer;  // FIXME: use list<Buffer> as output buffer.
  void* m_context_ptr;
  ZeroCopierPtr m_zero_copier_ptr;
  // FIXME: creation_time, last_receive_time
  //        bytes_received, bytes_sent
};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

}  // namespace flute

#endif  // FLUTE_NET_TCPCONNECTION_H
