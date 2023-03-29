
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_HTTP_HTTPSERVER_H
#define FLUTE_NET_HTTP_HTTPSERVER_H

#include <flute/net/TcpServer.h>

namespace flute {

class HttpRequest;
class HttpResponse;

/// A simple embeddable HTTP server designed for report status of a program.
/// It is not a fully HTTP 1.1 compliant server, but provides minimum features
/// that can communicate with HttpClient and Web browser.
/// It is synchronous, just like Java Servlet.
class HttpServer : noncopyable {
 public:
  typedef std::function<void(const HttpRequest&, HttpResponse*)>
      ReponseCallback;

  HttpServer(Reactor* reactor, const InetAddress& listenAddr,
             const string& name,
             TcpServer::Option option = TcpServer::kNoReusePort);

  Reactor* get_reactor() const { return m_tcp_server.get_listen_reactor(); }

  /// Not thread safe, callback be registered before calling start().
  void set_response_callback(const ReponseCallback& cb) {
    m_response_callback = cb;
  }

  void set_thread_num(int num_threads) {
    m_tcp_server.set_reactor_pool_size(num_threads);
  }

  void start();

 private:
  void on_connection(const TcpConnectionPtr& conn);
  void default_on_request(const TcpConnectionPtr& conn, Buffer* buf,
                          Timestamp receiveTime);
  void on_good_request(const TcpConnectionPtr&, const HttpRequest&);

  TcpServer m_tcp_server;
  ReponseCallback m_response_callback;
};

}  // namespace flute

#endif  // FLUTE_NET_HTTP_HTTPSERVER_H
