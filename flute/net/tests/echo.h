#ifndef FLUTE_EXAMPLES_SIMPLE_ECHO_ECHO_H
#define FLUTE_EXAMPLES_SIMPLE_ECHO_ECHO_H

#include <flute/net/TcpServer.h>

// RFC 862
class EchoServer {
 public:
  EchoServer(flute::Reactor* reactor, const flute::InetAddress& listenAddr);

  void start();  // calls m_tcp_server.start();

 private:
  void on_conn(const flute::TcpConnectionPtr& conn);

  void on_message(const flute::TcpConnectionPtr& conn, flute::Buffer* buf,
                 flute::Timestamp time);

  flute::TcpServer m_tcp_server;
};

#endif  // FLUTE_EXAMPLES_SIMPLE_ECHO_ECHO_H
