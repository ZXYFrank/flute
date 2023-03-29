#include <fcntl.h>
#include <flute/common/LogLine.h>
#include <flute/common/ZeroCopier.h>
#include <flute/common/types.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <string>

#include "stdio.h"

namespace flute {

size_t ZeroCopier::g_chunk_size = 1024;
ZeroCopier::CopyMode ZeroCopier::g_copy_mode = kZeroCopy;
string ZeroCopier::g_copy_mode_names[] = {"ZeroCopy", "MMap", "ReadWrite"};

ZeroCopier::ZeroCopier(const string& path)
    : m_failure_counter{0}, m_started{false} {
  // open the file
  m_source_available = false;
  int source_fd = ::open(path.c_str(), O_RDONLY);
  if (source_fd == -1) {
    char buf[100];
    sprintf(buf, "Cannot open file: %s!", path.c_str());
    perror(buf);
    return;
  }
  struct stat64 filestat;
  // get the file status
  if (fstat64(source_fd, &filestat) < 0) {
    perror("Invalid File!");
    return;
  }
  m_source_available = true;
  m_source_path = path;

  m_source_fd = source_fd;
  m_target_fd = -1;
  m_total_bytes = filestat.st_size;
  m_offset = 0;
}

// Can auto-adjust the chunk size.
size_t ZeroCopier::send_one_chunk() {
  if (!is_valid() || !has_started()) {
    return -1;
  }
  if (remaining_bytes() <= 0) return 0;
  ssize_t bytes_to_send =
      std::min(g_chunk_size, static_cast<size_t>(remaining_bytes()));
  ssize_t num_bytes_sent;
  errno = 0;
  switch (g_copy_mode) {
    case kZeroCopy:
      num_bytes_sent = send_one_chunk_zerocopy(bytes_to_send);
      break;
    case kMMap:
      num_bytes_sent = send_one_chunk_mmap(bytes_to_send);
      break;
    case kReadWrite:
      num_bytes_sent = send_one_chunk_rw(bytes_to_send);
      break;
    default:
      assert(false);
  }
  if (num_bytes_sent < 0) {
    m_failure_counter++;
    // HACK: ZeroCopier will try continously for kMaxTrial times.
    LOG_ERROR << "copy failed. trial= " << m_failure_counter;
    if (m_failure_counter >= kMaxTrial) {
      // The copier need to terminate itself, instead of sending that over and
      // over again
      mark_abort();
      LOG_ERROR << m_source_path << " send_one_chunk failed";
      return -1;
    }
  } else {
    m_failure_counter = 0;
  }
  return num_bytes_sent;
}

ssize_t ZeroCopier::send_one_chunk_zerocopy(size_t bytes_to_send) {
  ssize_t num_bytes_sent =
      sendfile(m_target_fd, m_source_fd, &m_offset, bytes_to_send);
  return num_bytes_sent;
}

ssize_t ZeroCopier::send_one_chunk_mmap(size_t bytes_to_send) {
  // HACK: offset must be a multiple of the page size as returned by
  // sysconf(_SC_PAGE_SIZE). https://man7.org/linux/man-pages/man2/mmap.2.html
  g_chunk_size = 1 * 1024;
  static size_t PAGESIZE = 4096;
  // HACK: Assume 4096 is the pagesize.
  off_t page_offset = (m_offset / PAGESIZE) * PAGESIZE;
  off_t gap = m_offset - page_offset;
  size_t len = gap + bytes_to_send;
  errno = 0;
  void* portion_ptr =
      mmap(NULL, len, PROT_READ, MAP_PRIVATE, m_source_fd, page_offset);
  if (errno != 0) {
    LOG_ERROR << "mmap";
    munmap(portion_ptr, len);
    return -1;
  }
  void* start = static_cast<void*>(static_cast<char*>(portion_ptr) + gap);
  errno = 0;
  ssize_t bytes_sent = send(m_target_fd, start, bytes_to_send, 0);
  if (bytes_sent > 0) {
    m_offset += bytes_sent;
  } else {
    LOG_ERROR << "send";
    return -1;
  }
  errno = 0;
  munmap(portion_ptr, bytes_to_send);
  if (errno != 0) {
    LOG_ERROR << "munmap";
    return -1;
  }
  return bytes_sent;
}

ssize_t ZeroCopier::send_one_chunk_rw(size_t bytes_to_send) {
  static char* buffer = static_cast<char*>(std::malloc(g_chunk_size));
  // create a shared pointer that manages the buffer
  static std::shared_ptr<char> buffer_ptr(buffer, std::free);
  ssize_t bytes_read = ::read(m_source_fd, buffer, bytes_to_send);
  if (errno != 0) {
    LOG_ERROR << "read error";
    return -1;
  }
  if (bytes_read == -1)
    return -1;
  else {
    ssize_t bytes_sent = write(m_target_fd, buffer, bytes_read);
    if (bytes_sent > 0) {
      m_offset += bytes_sent;
    }
    return bytes_sent;
  }
}

ZeroCopier::~ZeroCopier() {
  LOG_TRACE << "Zero Copier for " << m_source_path << " Deconstructed.";
  ::close(m_source_fd);
}

}  // namespace flute
