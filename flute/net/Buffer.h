
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_BUFFER_H
#define FLUTE_NET_BUFFER_H

#include <assert.h>
#include <flute/common/StringPiece.h>
#include <flute/common/types.h>
#include <flute/net/Endian.h>
#include <string.h>

#include <algorithm>
#include <vector>
// #include <unistd.h>  // ssize_t

namespace flute {

static const char* const kCRLF = "\r\n";

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
/// This class could be used for
/// Prependable->prepend() | CONTENT(READABLE)->peek_base() | Writable->append()
/// 0 <= reader_idx <= writerIndex <= size
// FIXME: Buffer is not thread safe. But that's fine, because buffer is always
// held by a certain thread. All IO operation of a certain thread will be
// executed sequentially on in certain Loop.
class Buffer {
 public:
  static const size_t kCheapPrepend = 8;
  static const size_t kInitialSize = 1024;

  explicit Buffer(size_t initialSize = kInitialSize)
      : m_buffer(kCheapPrepend + initialSize),
        m_read_idx(kCheapPrepend),
        m_write_idx(kCheapPrepend) {
    assert(content_bytes_len() == 0);
    assert(writable_bytes_len() == initialSize);
    assert(prependable_bytes_len() == kCheapPrepend);
  }

  void swap(Buffer& rhs) {
    m_buffer.swap(rhs.m_buffer);
    std::swap(m_read_idx, rhs.m_read_idx);
    std::swap(m_write_idx, rhs.m_write_idx);
  }

  size_t content_bytes_len() const { return m_write_idx - m_read_idx; }

  size_t writable_bytes_len() const { return m_buffer.size() - m_write_idx; }

  size_t prependable_bytes_len() const { return m_read_idx; }

  const char* peek_base() const { return begin() + m_read_idx; }

  const char* find_CRLF() const {
    // FIXME: replace with memmem()?
    const char* crlf = std::search(peek_base(), write_base(), kCRLF, kCRLF + 2);
    return crlf == write_base() ? NULL : crlf;
  }

  const char* find_CRLF(const char* start) const {
    assert(peek_base() <= start);
    assert(start <= write_base());
    // FIXME: replace with memmem()?
    const char* crlf = std::search(start, write_base(), kCRLF, kCRLF + 2);
    return crlf == write_base() ? NULL : crlf;
  }

  const char* find_EOL() const {
    const void* eol = memchr(peek_base(), '\n', content_bytes_len());
    return static_cast<const char*>(eol);
  }

  const char* find_EOL(const char* start) const {
    assert(peek_base() <= start);
    assert(start <= write_base());
    const void* eol = memchr(start, '\n', write_base() - start);
    return static_cast<const char*>(eol);
  }

  // retrieve some bytes of the head
  void retrieve(size_t len) {
    assert(len <= content_bytes_len());
    if (len < content_bytes_len()) {
      m_read_idx += len;
    } else {
      retrieve_all();
    }
  }

  void retrieve_until(const char* end) {
    assert(peek_base() <= end);
    assert(end <= write_base());
    retrieve(end - peek_base());
  }

  void retrieve_int64() { retrieve(sizeof(int64_t)); }

  void retrieve_int32() { retrieve(sizeof(int32_t)); }

  void retrieve_int16() { retrieve(sizeof(int16_t)); }

  void retrieve_int8() { retrieve(sizeof(int8_t)); }

  void retrieve_all() {
    m_read_idx = kCheapPrepend;
    m_write_idx = kCheapPrepend;
  }

  string readout_all_as_string() {
    return readout_as_string(content_bytes_len());
  }

  string readout_as_string(size_t len) {
    assert(len <= content_bytes_len());
    string result(peek_base(), len);
    retrieve(len);
    return result;
  }

  StringPiece as_string_piece() const {
    return StringPiece(peek_base(), static_cast<int>(content_bytes_len()));
  }

  string as_string() const {
    return string(peek_base(), static_cast<int>(content_bytes_len()));
  }

  string peek_last_as_string(size_t len) const {
    if (len > content_bytes_len()) {
      return "";
    }
    const char* start = peek_base() + (content_bytes_len() - len);
    return string(start, len);
  }

  void append(const StringPiece& str) { append(str.data(), str.size()); }

  void append(const char* const data, size_t len) {
    ensure_writable_len(len);
    std::copy(data, data + len, write_base());
    mark_written(len);
  }

  void write(const void* const data, size_t len) {
    append(static_cast<const char*>(data), len);
  }

  void ensure_writable_len(size_t len) {
    if (writable_bytes_len() < len) {
      make_space(len);
    }
    assert(writable_bytes_len() >= len);
  }

  char* write_base() { return begin() + m_write_idx; }

  const char* write_base() const { return begin() + m_write_idx; }

  void mark_written(size_t len) {
    assert(len <= writable_bytes_len());
    m_write_idx += len;
  }

  void unwrite(size_t len) {
    assert(len <= content_bytes_len());
    m_write_idx -= len;
  }

  ///
  /// Append int64_t using network endian
  ///
  void append_int64_net(int64_t x) {
    int64_t be64 = socket_ops::host_to_net64(x);
    write(&be64, sizeof be64);
  }

  void append_int32_net(int32_t x) {
    int32_t be32 = socket_ops::host_to_net32(x);
    write(&be32, sizeof be32);
  }

  void append_int16_net(int16_t x) {
    int16_t be16 = socket_ops::host_to_net16(x);
    write(&be16, sizeof be16);
  }

  void append_int8_net(int8_t x) { write(&x, sizeof x); }

  int64_t readout_int64_host() {
    int64_t result = peek_int64_host();
    retrieve_int64();
    return result;
  }

  int32_t readout_int32_host() {
    int32_t result = peek_int32_host();
    retrieve_int32();
    return result;
  }

  int16_t readout_int16_host() {
    int16_t result = peek_int16_host();
    retrieve_int16();
    return result;
  }

  int8_t readout_int8_host() {
    int8_t result = peek_int8_host();
    retrieve_int8();
    return result;
  }

  int64_t peek_int64_host() const {
    assert(content_bytes_len() >= sizeof(int64_t));
    int64_t be64 = 0;
    ::memcpy(&be64, peek_base(), sizeof be64);
    return socket_ops::net_to_host64(be64);
  }

  int32_t peek_int32_host() const {
    assert(content_bytes_len() >= sizeof(int32_t));
    int32_t be32 = 0;
    ::memcpy(&be32, peek_base(), sizeof be32);
    return socket_ops::net_to_host32(be32);
  }

  int16_t peek_int16_host() const {
    assert(content_bytes_len() >= sizeof(int16_t));
    int16_t be16 = 0;
    ::memcpy(&be16, peek_base(), sizeof be16);
    return socket_ops::net_to_host16(be16);
  }

  int8_t peek_int8_host() const {
    assert(content_bytes_len() >= sizeof(int8_t));
    int8_t x = *peek_base();
    return x;
  }

  void prepend_int64_net(int64_t x) {
    int64_t be64 = socket_ops::host_to_net64(x);
    prepend(&be64, sizeof be64);
  }

  void prepend_int32_net(int32_t x) {
    int32_t be32 = socket_ops::host_to_net32(x);
    prepend(&be32, sizeof be32);
  }

  void prepend_int16_net(int16_t x) {
    int16_t be16 = socket_ops::host_to_net16(x);
    prepend(&be16, sizeof be16);
  }

  void prepend_int8_net(int8_t x) { prepend(&x, sizeof x); }

  // Prepend some chars ahead of read_idx(readable content). This is useful for
  // adding headers for variable-length string.
  void prepend(const void* /*restrict*/ data, size_t len) {
    assert(len <= prependable_bytes_len());
    m_read_idx -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d + len, begin() + m_read_idx);
  }

  void shrink(size_t reserve) {
    // FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
    Buffer other;
    other.ensure_writable_len(content_bytes_len() + reserve);
    other.append(as_string_piece());
    swap(other);
  }

  size_t buffer_capacity() const { return m_buffer.capacity(); }

  // read all or nothing from fd to the buffer as payload
  ssize_t read_all_from(int fd, int* savedErrno);

 private:
  char* begin() { return &*m_buffer.begin(); }

  const char* begin() const { return &*m_buffer.begin(); }

  void make_space(size_t len) {
    if (writable_bytes_len() + prependable_bytes_len() < len + kCheapPrepend) {
      // Cannot make space by moving current payload. Resize the vector, add
      // space to write idx
      // FIXME: Here is an ignorable redundant operation: vector.resize will
      // call memset.
      m_buffer.resize(m_write_idx + len);
    } else {
      // move readable data to the front, make space inside buffer
      assert(kCheapPrepend < m_read_idx);
      size_t readable = content_bytes_len();
      std::copy(begin() + m_read_idx, begin() + m_write_idx,
                begin() + kCheapPrepend);
      m_read_idx = kCheapPrepend;
      m_write_idx = m_read_idx + readable;
      assert(readable == content_bytes_len());
    }
  }

 private:
  std::vector<char> m_buffer;
  // indexes are of int. When iterator changes, we can still refer to the data
  // in a contiguous space using begin() and idx.
  size_t m_read_idx;
  size_t m_write_idx;
};

}  // namespace flute

#endif  // FLUTE_NET_BUFFER_H
