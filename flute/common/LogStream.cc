#include <flute/common/LogStream.h>
#include <flute/common/Digital.h>

using namespace flute;

// Any array

LogStream& LogStream::operator<<(const void* p) {
  uintptr_t v = reinterpret_cast<uintptr_t>(p);
  if (m_buffer.avail() >= kMaxNumericCharNum) {
    char* buf = m_buffer.cur_pos();
    buf[0] = '0';
    buf[1] = 'x';
    size_t len = hex_to_string(buf + 2, v);
    m_buffer.cur_add(len + 2);
  }
  return *this;
}

// Integers

template <typename T>
void LogStream::log_integer(T v) {
  if (m_buffer.avail() >= kMaxNumericCharNum) {
    size_t len = integer_to_string(m_buffer.cur_pos(), v);
    m_buffer.cur_add(len);
  }
}

LogStream& LogStream::operator<<(short v) {
  *this << static_cast<int>(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned short v) {
  *this << static_cast<unsigned int>(v);
  return *this;
}

LogStream& LogStream::operator<<(int v) {
  log_integer(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned int v) {
  log_integer(v);
  return *this;
}

LogStream& LogStream::operator<<(long v) {
  log_integer(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned long v) {
  log_integer(v);
  return *this;
}

LogStream& LogStream::operator<<(long long v) {
  log_integer(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned long long v) {
  log_integer(v);
  return *this;
}

// Double
LogStream& LogStream::operator<<(double v) {
  if (m_buffer.avail() >= kMaxNumericCharNum) {
    int len = snprintf(m_buffer.cur_pos(), kMaxNumericCharNum, "%.12g", v);
    m_buffer.cur_add(len);
  }
  return *this;
}