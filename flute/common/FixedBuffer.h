#ifndef FLUTE_COMMON_FIXEDBUFFER_H
#define FLUTE_COMMON_FIXEDBUFFER_H

#include <flute/common/StringPiece.h>
#include <flute/common/noncopyable.h>
#include <flute/common/types.h>

namespace flute {

template <int BUF_SIZE>
class FixedBuffer : noncopyable {
 public:
  FixedBuffer() {
    // CAUTION: m_cur is not initialized.
    m_cur = m_data;
    set_cookie(cookie_start);
  }

  ~FixedBuffer() { set_cookie(cookie_end); }

  void append(const char* buf, size_t len) {
    if (implicit_cast<size_t>(avail()) > len) {
      memcpy(m_cur, buf, len);
      m_cur += len;
    }
  }

  const char* data() const { return m_data; }
  int length() const { return static_cast<int>(m_cur - m_data); }

  char* cur_pos() { return m_cur; }
  void cur_add(size_t len) { m_cur += len; }
  int avail() const { return static_cast<int>(data_end() - m_cur); }

  void reset_cur() { m_cur = m_data; }
  inline void reset_buffer() { mem_zero(m_data, sizeof(m_data)); }
  inline void reset() {
    reset_cur();
    reset_buffer();
  }

  // for used by GDB
  const char* debug_string();
  void set_cookie(void (*cookie)()) { m_cookie = cookie; }

  // for used by unit test
  string to_string() const { return string(m_data, length()); }
  StringPiece to_string_piece() const { return StringPiece(m_data, length()); }

 private:
  const char* data_end() const { return m_data + sizeof(m_data); }

  static void cookie_start(){};
  static void cookie_end(){};

  void (*m_cookie)();

  char m_data[BUF_SIZE];
  char* m_cur;
};
}  // namespace flute

#endif  // FLUTE_COMMON_FIXEDBUFFER_H