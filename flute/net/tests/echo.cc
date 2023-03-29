
#include <flute/common/LogLine.h>
#include <flute/common/CurrentThread.h>
#include <flute/net/Reactor.h>

#include "echo.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

using namespace flute;

EchoServer::EchoServer(flute::Reactor* reactor,
                       const flute::InetAddress& listenAddr)
    : m_tcp_server(reactor, listenAddr, "EchoServer") {
  m_tcp_server.set_conn_callback(std::bind(&EchoServer::on_conn, this, _1));
  m_tcp_server.set_message_callback(
      std::bind(&EchoServer::on_message, this, _1, _2, _3));
}

void EchoServer::start() { m_tcp_server.start(); }

void EchoServer::on_conn(const flute::TcpConnectionPtr& conn) {
  LOG_INFO << "EchoServer - " << conn->peer_address().to_ip_port() << " -> "
           << conn->local_address().to_ip_port() << " is "
           << (conn->connected() ? "UP" : "DOWN");
}

void EchoServer::on_message(const flute::TcpConnectionPtr& conn,
                            flute::Buffer* buf, flute::Timestamp time) {
  flute::string msg(buf->readout_all_as_string());
  LOG_INFO << conn->name() << " echo " << msg.size() << " bytes, "
           << "data received at " << time.to_string();
  conn->send_string_piece(StringPiece(msg));
}

int main() {
  LOG_INFO << "pid = " << CurrentThread::tid();
  flute::Reactor reactor;
  flute::InetAddress listenAddr(2007);
  EchoServer server(&reactor, listenAddr);
  server.start();
  reactor.loop();
}
