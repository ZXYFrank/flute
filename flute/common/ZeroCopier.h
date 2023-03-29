#ifndef FLUTE_COMMON_ZEROCOPIER_H
#define FLUTE_COMMON_ZEROCOPIER_H
#include <flute/common/types.h>
#include <flute/net/Channel.h>
#include <sys/sendfile.h>

#include <memory>
#include <string>

namespace flute {

class ZeroCopier;

typedef std::shared_ptr<ZeroCopier> ZeroCopierPtr;

class ZeroCopier {
 public:
  enum CopyMode { kZeroCopy, kMMap, kReadWrite };
  static string g_copy_mode_names[];
  static const int kMaxTrial = 100;

  static size_t g_chunk_size;
  static CopyMode g_copy_mode;
  ZeroCopier() = delete;
  ZeroCopier(const string& path);
  ~ZeroCopier();

  bool has_finished() const {
    if (!is_valid()) {
      return true;
    } else if (!m_started) {
      return true;
    } else {
      assert(m_source_available);
      return (static_cast<size_t>(m_offset) == m_total_bytes);
    }
  }
  size_t source_size() const { return m_total_bytes; }

  bool is_valid() const { return m_source_available && (m_target_fd != -1); }

  // send with best effort
  // size_t not ssize_t because it may return -1 as err
  size_t send_one_chunk();
  ssize_t remaining_bytes() const { return m_total_bytes - m_offset; }

  void set_target_fd(int target_fd) { m_target_fd = target_fd; }
  static void set_chunk_size(size_t chunk_size) { g_chunk_size = chunk_size; }
  static void set_g_copy_mode(CopyMode on) { g_copy_mode = on; }

  // HACK: Lie to the caller that it has finished.
  void mark_abort() {
    m_total_bytes = 0;
    m_offset = 0;
  }

  bool to_abort() { return m_total_bytes == 0 && m_offset == 0; }
  void start() { m_started = true; }
  bool has_started() const { return m_started; }

  string& copymode_to_string() const {
    return g_copy_mode_names[static_cast<int>(g_copy_mode)];
  }

  const string& source_path() { return m_source_path; }

 private:
  ssize_t send_one_chunk_zerocopy(size_t bytes_to_send);
  ssize_t send_one_chunk_mmap(size_t bytes_to_send);
  ssize_t send_one_chunk_rw(size_t bytes_to_send);

 private:
  string m_source_path;
  int m_source_fd;
  int m_target_fd;
  int m_failure_counter;
  // If any file error occurs, it will become false;
  bool m_source_available;
  size_t m_total_bytes;
  off_t m_offset;
  bool m_started;
};

}  // namespace flute

#endif  // FLUTE_COMMON_ZEROCOPIER_H