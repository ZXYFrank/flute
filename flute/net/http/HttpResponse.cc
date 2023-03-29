#include <flute/net/http/HttpResponse.h>
#include <flute/net/Buffer.h>
#include <flute/common/LogLine.h>

#include <stdio.h>
#include <memory>

namespace flute {

void HttpResponse::append_to_buffer(Buffer* out_buf) const {
  char buf[32];
  snprintf(buf, sizeof buf, "HTTP/1.1 %d ", m_status_code);
  out_buf->append(buf);
  out_buf->append(m_status_message);
  out_buf->append("\r\n");

  if (m_will_close) {
    out_buf->append("Connection: close\r\n");
  } else {
    snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", content_length());
    out_buf->append(buf);
    // FIXME: how to handle keep-alive
    out_buf->append("Connection: Keep-Alive\r\n");
  }

  for (const auto& header : m_headers) {
    out_buf->append(header.first);
    out_buf->append(": ");
    out_buf->append(header.second);
    out_buf->append("\r\n");
  }

  out_buf->append("\r\n");
  out_buf->append(m_body);
}

size_t HttpResponse::content_length() const {
  size_t size;
  if (m_response_body_type == kHtml) {
    size = m_body.size();
    if (size == 0) {
      LOG_WARN << "Body is Empty!";
    }
    return size;
  } else if (m_response_body_type == kJPEG) {
    return m_zero_copier_ptr->source_size();
  } else {
    // FIXME: Unsupported types
    return 0;
  }
}

HttpResponse::HttpResponsePtr HttpResponse::response_404() {
  HttpResponsePtr resp_ptr = std::make_shared<HttpResponse>(true);
  resp_ptr->set_status_code(HttpResponse::k404NotFound);
  resp_ptr->set_status_message("Not Found");
  return resp_ptr;
}
HttpResponse::HttpResponsePtr HttpResponse::response_400() {
  HttpResponsePtr resp_ptr = std::make_shared<HttpResponse>(true);
  resp_ptr->set_status_code(HttpResponse::k400BadRequest);
  resp_ptr->set_status_message("Bad Request");
  return resp_ptr;
}
}  // namespace flute