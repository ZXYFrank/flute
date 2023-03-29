#ifndef FLUTE_NET_HTTP_HTTPREQUEST_H
#define FLUTE_NET_HTTP_HTTPREQUEST_H

#include <flute/common/Timestamp.h>
#include <flute/common/types.h>

#include <map>
#include <assert.h>
#include <stdio.h>

namespace flute {

class HttpRequest {
 public:
  enum Method { kInvalid, kGet, kPost, kHead, kPut, kDelete };
  enum Version { kUnknown, kHttp10, kHttp11 };

  HttpRequest() : m_method(kInvalid), m_version(kUnknown) {}

  void setVersion(Version v) { m_version = v; }

  Version get_version() const { return m_version; }

  bool set_method(const char* start, const char* end) {
    assert(m_method == kInvalid);
    string m(start, end);
    if (m == "GET") {
      m_method = kGet;
    } else if (m == "POST") {
      m_method = kPost;
    } else if (m == "HEAD") {
      m_method = kHead;
    } else if (m == "PUT") {
      m_method = kPut;
    } else if (m == "DELETE") {
      m_method = kDelete;
    } else {
      m_method = kInvalid;
    }
    return m_method != kInvalid;
  }

  Method method() const { return m_method; }

  const char* method_string() const {
    const char* result = "UNKNOWN";
    switch (m_method) {
      case kGet:
        result = "GET";
        break;
      case kPost:
        result = "POST";
        break;
      case kHead:
        result = "HEAD";
        break;
      case kPut:
        result = "PUT";
        break;
      case kDelete:
        result = "DELETE";
        break;
      default:
        break;
    }
    return result;
  }

  void set_path(const char* start, const char* end) {
    m_request_path.assign(start, end);
  }

  const string& path() const { return m_request_path; }

  void set_query(const char* start, const char* end) {
    m_query.assign(start, end);
  }

  const string& query() const { return m_query; }

  void set_receive_time(Timestamp t) { m_receive_time = t; }

  Timestamp receive_time() const { return m_receive_time; }

  void add_header(const char* start, const char* colon, const char* end) {
    string field(start, colon);
    ++colon;
    while (colon < end && isspace(*colon)) {
      ++colon;
    }
    string value(colon, end);
    while (!value.empty() && isspace(value[value.size() - 1])) {
      value.resize(value.size() - 1);
    }
    m_headers[field] = value;
  }

  string get_header(const string& field) const {
    string result;
    std::map<string, string>::const_iterator it = m_headers.find(field);
    if (it != m_headers.end()) {
      result = it->second;
    }
    return result;
  }

  const std::map<string, string>& headers() const { return m_headers; }

  void swap(HttpRequest& that) {
    std::swap(m_method, that.m_method);
    std::swap(m_version, that.m_version);
    m_request_path.swap(that.m_request_path);
    m_query.swap(that.m_query);
    m_receive_time.swap(that.m_receive_time);
    m_headers.swap(that.m_headers);
  }

 private:
  Method m_method;
  Version m_version;
  string m_request_path;
  string m_query;
  Timestamp m_receive_time;
  std::map<string, string> m_headers;
};

}  // namespace flute

#endif  // FLUTE_NET_HTTP_HTTPREQUEST_H
