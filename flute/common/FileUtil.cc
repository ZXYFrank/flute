#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <flute/common/FileUtil.h>
#include <flute/common/LogLine.h>  // strerror_tl
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

namespace flute {

FileUtil::AppendFile::AppendFile(StringArg filename)
    : m_fp(::fopen(filename.c_str(), "ae")),  // 'e' for O_CLOEXEC
      m_written_bytes(0) {
  assert(m_fp);
  ::setbuffer(m_fp, m_buffer, sizeof m_buffer);
  // posix_fadvise POSIX_FADV_DONTNEED ?
}

FileUtil::AppendFile::~AppendFile() { ::fclose(m_fp); }

void FileUtil::AppendFile::append(const char* logline, const size_t len) {
  size_t n = write(logline, len);
  size_t remain = len - n;
  while (remain > 0) {
    size_t x = write(logline + n, remain);
    if (x == 0) {
      int err = ferror(m_fp);
      if (err) {
        fprintf(stderr, "AppendFile::append() failed %s\n", strerror_tl(err));
      }
      break;
    }
    n += x;
    remain = len - n;  // remain -= x
  }

  m_written_bytes += len;
}

void FileUtil::AppendFile::flush() { ::fflush(m_fp); }

size_t FileUtil::AppendFile::write(const char* logline, size_t len) {
  // #undef fwrite_unlocked
  return ::fwrite_unlocked(logline, 1, len, m_fp);
}

FileUtil::ReadSmallFile::ReadSmallFile(StringArg filename)
    : m_fd(::open(filename.c_str(), O_RDONLY | O_CLOEXEC)), m_err(0) {
  m_buf[0] = '\0';
  if (m_fd < 0) {
    m_err = errno;
  }
}

FileUtil::ReadSmallFile::~ReadSmallFile() {
  if (m_fd >= 0) {
    ::close(m_fd);  // FIXME: check EINTR
  }
}

// return errno
template <typename String>
int FileUtil::ReadSmallFile::read_to_string(int maxSize, String* content,
                                            int64_t* fileSize,
                                            int64_t* modifyTime,
                                            int64_t* createTime) {
  static_assert(sizeof(off_t) == 8, "_FILE_OFFSET_BITS = 64");
  assert(content != NULL);
  int err = m_err;
  if (m_fd >= 0) {
    content->clear();

    if (fileSize) {
      struct stat statbuf;
      if (::fstat(m_fd, &statbuf) == 0) {
        if (S_ISREG(statbuf.st_mode)) {
          *fileSize = statbuf.st_size;
          content->reserve(static_cast<int>(
              std::min(implicit_cast<int64_t>(maxSize), *fileSize)));
        } else if (S_ISDIR(statbuf.st_mode)) {
          err = EISDIR;
        }
        if (modifyTime) {
          *modifyTime = statbuf.st_mtime;
        }
        if (createTime) {
          *createTime = statbuf.st_ctime;
        }
      } else {
        err = errno;
      }
    }

    while (content->size() < implicit_cast<size_t>(maxSize)) {
      size_t toRead = std::min(implicit_cast<size_t>(maxSize) - content->size(),
                               sizeof(m_buf));
      ssize_t n = ::read(m_fd, m_buf, toRead);
      if (n > 0) {
        content->append(m_buf, n);
      } else {
        if (n < 0) {
          err = errno;
        }
        break;
      }
    }
  }
  return err;
}

int FileUtil::ReadSmallFile::read_to_buffer(int* size) {
  int err = m_err;
  if (m_fd >= 0) {
    ssize_t n = ::pread(m_fd, m_buf, sizeof(m_buf) - 1, 0);
    if (n >= 0) {
      if (size) {
        *size = static_cast<int>(n);
      }
      m_buf[n] = '\0';
    } else {
      err = errno;
    }
  }
  return err;
}

template int FileUtil::readFile(StringArg filename, int maxSize,
                                string* content, int64_t*, int64_t*, int64_t*);

template int FileUtil::ReadSmallFile::read_to_string(int maxSize,
                                                     string* content, int64_t*,
                                                     int64_t*, int64_t*);

}  // namespace flute