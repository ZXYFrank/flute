// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_COMMON_FILEUTIL_H
#define FLUTE_COMMON_FILEUTIL_H

#include <flute/common/StringPiece.h>
#include <flute/common/noncopyable.h>
#include <sys/types.h>  // for off_t

namespace flute {
namespace FileUtil {

// read small file < 64KB
class ReadSmallFile : noncopyable {
 public:
  ReadSmallFile(StringArg filename);
  ~ReadSmallFile();

  // return errno
  template <typename String>
  int read_to_string(int maxSize, String* content, int64_t* fileSize,
                     int64_t* modifyTime, int64_t* createTime);

  /// Read at maxium kBufferSize into buf_
  // return errno
  int read_to_buffer(int* size);

  const char* buffer() const { return m_buf; }

  static const int kBufferSize = 64 * 1024;

 private:
  int m_fd;
  int m_err;
  char m_buf[kBufferSize];
};

// read the file content, returns errno if error happens.
template <typename String>
int read_file(StringArg filename, int maxSize, String* content,
              int64_t* fileSize = NULL, int64_t* modifyTime = NULL,
              int64_t* createTime = NULL) {
  ReadSmallFile file(filename);
  return file.read_to_string(maxSize, content, fileSize, modifyTime,
                             createTime);
}

// not thread safe
class AppendFile : noncopyable {
 public:
  explicit AppendFile(StringArg filename);

  ~AppendFile();

  void append(const char* logline, size_t len);

  void flush();

  off_t written_bytes() const { return m_written_bytes; }

 private:
  size_t write(const char* logline, size_t len);

  FILE* m_fp;
  char m_buffer[64 * 1024];
  off_t m_written_bytes;
};

}  // namespace FileUtil
}  // namespace flute

#endif  // FLUTE_COMMON_FILEUTIL_H
