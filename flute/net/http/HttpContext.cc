#include <flute/net/Buffer.h>
#include <flute/net/http/HttpContext.h>

namespace flute {

bool HttpContext::process_request_line(const char* begin, const char* end) {
  bool succeed = false;
  const char* start = begin;
  const char* space = std::find(start, end, ' ');
  if (space != end && m_request.set_method(start, space)) {
    start = space + 1;
    space = std::find(start, end, ' ');
    if (space != end) {
      const char* question = std::find(start, space, '?');
      if (question != space) {
        m_request.set_path(start, question);
        m_request.set_query(question, space);
      } else {
        m_request.set_path(start, space);
      }
      start = space + 1;
      succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
      if (succeed) {
        if (*(end - 1) == '1') {
          m_request.setVersion(HttpRequest::kHttp11);
        } else if (*(end - 1) == '0') {
          m_request.setVersion(HttpRequest::kHttp10);
        } else {
          succeed = false;
        }
      }
    }
  }
  return succeed;
}

// This will consume the content in buffer and try to modify the state of
// HttpContext. return false if any error
// Note that this function will change the state of the buffer, hence it could
// not be called twice on the same buffer
bool HttpContext::parse_request(Buffer* buf, Timestamp receiveTime) {
  bool ok = false;
  bool has_more = true;
  while (has_more) {
    if (m_state == kExpectRequestLine) {
      const char* crlf = buf->find_CRLF();
      if (crlf) {
        ok = process_request_line(buf->peek_base(), crlf);
        if (ok) {
          m_request.set_receive_time(receiveTime);
          buf->retrieve_until(crlf + 2);
          m_state = kExpectHeaders;
        } else {
          has_more = false;
        }
      } else {
        has_more = false;
      }
    } else if (m_state == kExpectHeaders) {
      const char* crlf = buf->find_CRLF();
      if (crlf) {
        const char* colon = std::find(buf->peek_base(), crlf, ':');
        if (colon != crlf) {
          m_request.add_header(buf->peek_base(), colon, crlf);
        } else {
          // empty line, end of header
          // FIXME:
          m_state = kGotAll;
          has_more = false;
        }
        buf->retrieve_until(crlf + 2);
      } else {
        has_more = false;
      }
    } else if (m_state == kExpectBody) {
      // TODO: expect body. kExpectBody unused now.
    } else {
      return false;
    }
  }
  return ok;
}

}  // namespace flute