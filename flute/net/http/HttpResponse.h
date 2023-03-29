
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_HTTP_HTTPRESPONSE_H
#define FLUTE_NET_HTTP_HTTPRESPONSE_H

#include <flute/common/ZeroCopier.h>
#include <flute/common/types.h>

#include <map>
#include <memory>

namespace flute {

enum HttpResponseBodyType { kHtml, kJPEG, kUnSupported };

class Buffer;

class HttpResponse {
 public:
  enum HttpStatusCode {
    kUnknown,
    k200Ok = 200,
    k301MovedPermanently = 301,
    k400BadRequest = 400,
    k404NotFound = 404,
  };
  typedef std::shared_ptr<HttpResponse> HttpResponsePtr;

  explicit HttpResponse(bool close)
      : m_status_code(kUnknown),
        m_will_close(close),
        m_response_body_type(kHtml) {}

  void set_status_code(HttpStatusCode code) { m_status_code = code; }

  void set_status_message(const string& message) { m_status_message = message; }

  void set_close_conn(bool on) { m_will_close = on; }

  bool will_close() const { return m_will_close; }

  void set_content_type(const string& contentType) {
    add_header("Content-Type", contentType);
  }

  // FIXME: replace string with StringPiece
  void add_header(const string& key, const string& value) {
    m_headers[key] = value;
  }

  void set_body(const string& body) {
    m_body = body;
    m_response_body_type = kHtml;
    m_zero_copier_ptr.reset();
  }

  void set_zerocopy(const string& path) {
    m_zero_copier_ptr = ZeroCopierPtr(new ZeroCopier(path));
    m_response_body_type = kJPEG;
    m_body = "";
  }

  void append_to_buffer(Buffer* out_buf) const;

  // common messages
  static HttpResponsePtr response_400();
  static HttpResponsePtr response_404();

  // When the content is completely written into the buffer, content length is
  // m_body.size(). However, when there are files to send, m_body is empty.
  size_t content_length() const;
  ZeroCopierPtr zero_copier_ptr() const { return m_zero_copier_ptr; }
  HttpResponseBodyType response_type() const { return m_response_body_type; }

 private:
  std::map<string, string> m_headers;
  HttpStatusCode m_status_code;
  // FIXME: add http version
  string m_status_message;
  ZeroCopierPtr m_zero_copier_ptr;
  bool m_will_close;
  string m_body;
  HttpResponseBodyType m_response_body_type;
};

}  // namespace flute

#endif  // FLUTE_NET_HTTP_HTTPRESPONSE_H
