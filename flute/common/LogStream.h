#ifndef FLUTE_COMMON_LOGSTREAM_H
#define FLUTE_COMMON_LOGSTREAM_H

#include <flute/common/FixedBuffer.h>

#include <string.h>
#include <assert.h>

namespace flute {

const int kLogBufferSize = 1000;
const int kMaxNumericCharNum = 32;  // max char length for all numeric type

typedef FixedBuffer<kLogBufferSize> LogBuffer;

// Helper Anonymous Namespace
namespace {

// Util class, format a numeric value
class FormattedString {
 public:
  // Inference the value type during compilation
  template <typename T>
  FormattedString(const char* fmt, T val) {
    static_assert(std::is_arithmetic<T>::value == true,
                  "must be arithmetic type");
    mem_zero(m_chars, 32);
    // CAUTION: m_len_0 means the length include the ending '\0'
    m_len_0 = snprintf(m_chars, sizeof(m_chars), fmt, val);
    assert(static_cast<size_t>(m_len_0) <= sizeof(m_chars));
  }

  const char* data() const { return m_chars; }
  int length_0() const { return m_len_0; }

 private:
  char m_chars[32];
  int m_len_0;
};

}  // namespace

class LogStream {
 public:
  void append(const char* data, int len) { m_buffer.append(data, len); }

  // Overload << for bool
  LogStream& operator<<(bool v) {
    m_buffer.append(v ? "1" : "0", 1);
    return *this;
  }
  // Overload << for void*
  LogStream& operator<<(const void*);

  // Overload << for numerics
  template <typename T>
  void log_integer(T);

  LogStream& operator<<(short);
  LogStream& operator<<(unsigned short);
  LogStream& operator<<(int);
  LogStream& operator<<(unsigned int);
  LogStream& operator<<(long);
  LogStream& operator<<(unsigned long);
  LogStream& operator<<(long long);
  LogStream& operator<<(unsigned long long);

  LogStream& operator<<(double);
  LogStream& operator<<(float v) {
    *this << static_cast<double>(v);
    return *this;
  }

  // Overload << for strings
  LogStream& operator<<(char v) {
    m_buffer.append(&v, 1);
    return *this;
  }
  LogStream& operator<<(const char* str) {
    if (str) {
      m_buffer.append(str, strlen(str));
    } else {
      m_buffer.append("(null)", 6);
    }
    return *this;
  }

  LogStream& operator<<(const unsigned char* str) {
    return operator<<(reinterpret_cast<const char*>(str));
  }

  LogStream& operator<<(const string& v) {
    m_buffer.append(v.c_str(), v.size());
    return *this;
  }

  LogStream& operator<<(const LogBuffer& v) {
    *this << v.to_string_piece();
    return *this;
  }

  LogStream& operator<<(const StringPiece& v) {
    m_buffer.append(v.data(), v.size());
    return *this;
  }

 public:
  LogBuffer m_buffer;
};

}  // namespace flute

#endif  // FLUTE_COMMON_LOGSTREAM_H