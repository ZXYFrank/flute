
//

#include <flute/common/LogLine.h>
#include <flute/net/http/HttpContext.h>
#include <flute/net/http/HttpRequest.h>
#include <flute/net/http/HttpResponse.h>
#include <flute/net/http/HttpServer.h>

namespace flute {

namespace detail {

void dummy_404_callback(const HttpRequest&, HttpResponse* resp) {
  resp->set_status_code(HttpResponse::k404NotFound);
  resp->set_status_message("Not Found");
  resp->set_close_conn(true);
}

}  // namespace detail

HttpServer::HttpServer(Reactor* reactor, const InetAddress& listenAddr,
                       const string& name, TcpServer::Option option)
    : m_tcp_server(reactor, listenAddr, name, option),
      m_response_callback(detail::dummy_404_callback) {
  m_tcp_server.set_conn_callback(
      std::bind(&HttpServer::on_connection, this, _1));
  m_tcp_server.set_message_callback(
      std::bind(&HttpServer::default_on_request, this, _1, _2, _3));
}

void HttpServer::start() {
  LOG_INFO << "HttpServer[" << m_tcp_server.name() << "] starts listenning on "
           << m_tcp_server.ip_port();
  m_tcp_server.start();
}

void HttpServer::on_connection(const TcpConnectionPtr& conn) {
  if (conn->connected()) {
    // FIXME: strongly coupled
    HttpContext* context = new HttpContext();
    conn->set_context_ptr(context);
  }
}

void HttpServer::default_on_request(const TcpConnectionPtr& conn, Buffer* buf,
                                    Timestamp receiveTime) {
  HttpContext* request_context =
      static_cast<HttpContext*>(conn->get_mutable_context_ptr());

  // ONGOING BUG: What if the received bytes are not the full content of the
  // HTTP request? Remember that TCP is byte stream.
  string end = buf->peek_last_as_string(4);
  if (end != "\r\n\r\n") {
    LOG_TRACE << "Incomplete html request" << buf->as_string();
    return;
  }
  if (!request_context->parse_request(buf, receiveTime)) {
    HttpResponse::HttpResponsePtr resp400 = HttpResponse::response_400();
    Buffer temp_buf;
    resp400->append_to_buffer(&temp_buf);
    conn->send_buffer(&temp_buf);
    // BUG: should tell whether 404 has been sent.
    conn->shutdown();
  }

  if (request_context->got_all()) {
    on_good_request(conn, request_context->request());
    // FIXME: ugly
    request_context->reset();
  }
}

// WARNING: This function is not thread safe. It works only because it's binded
// with certain TCPConnection
void HttpServer::on_good_request(const TcpConnectionPtr& conn,
                                 const HttpRequest& req) {
  const string& connection = req.get_header("Connection");
  bool close =
      connection == "close" ||
      (req.get_version() == HttpRequest::kHttp10 && connection != "Keep-Alive");
  HttpResponse response(close);
  // generate response
  m_response_callback(req, &response);
  Buffer buf;
  response.append_to_buffer(&buf);

  // If response is html, body data has been put into the buffer. And the
  // connection will copt that.
  conn->send_buffer(&buf);

  // It response is jpeg, the above step has only setup the headers. We need to
  // manually start sending the body data.
  if (response.response_type() == kJPEG) {
    conn->enable_zero_copy(response.zero_copier_ptr());
  }
  if (response.will_close()) {
    conn->shutdown();
  }
}

}  // namespace flute