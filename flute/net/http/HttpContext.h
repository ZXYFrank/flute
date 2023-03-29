#ifndef FLUTE_NET_HTTP_HTTPCONTEXT_H
#define FLUTE_NET_HTTP_HTTPCONTEXT_H

#include <flute/net/http/HttpRequest.h>

namespace flute {

class Buffer;

class HttpContext {
 public:
  enum HttpRequestParseState {
    kExpectRequestLine,
    kExpectHeaders,
    kExpectBody,
    kGotAll,
  };

  HttpContext() : m_state(kExpectRequestLine) {}

  // default copy-ctor, dtor and assignment are fine

  // return false if any error
  bool parse_request(Buffer* buf, Timestamp receiveTime);

  bool got_all() const { return m_state == kGotAll; }

  void reset() {
    m_state = kExpectRequestLine;
    HttpRequest dummy;
    // FIXME: ugly
    m_request.swap(dummy);
  }

  const HttpRequest& request() const { return m_request; }

  HttpRequest& request() { return m_request; }

 private:
  bool process_request_line(const char* begin, const char* end);

  HttpRequestParseState m_state;
  HttpRequest m_request;
};

}  // namespace flute

#endif  // FLUTE_NET_HTTP_HTTPCONTEXT_H
