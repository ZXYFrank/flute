
//

#include <errno.h>
#include <flute/net/Buffer.h>
#include <flute/net/SocketsOps.h>
#include <sys/uio.h>

namespace flute {

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;

ssize_t Buffer::read_all_from(int fd, int* saved_errno) {
  // saved an ioctl()/FIONREAD call to tell how much to read
  char extrabuf[65536];
  struct iovec io_vecs[2];
  const size_t writable = writable_bytes_len();
  io_vecs[0].iov_base = write_base();
  io_vecs[0].iov_len = writable;
  io_vecs[1].iov_base = extrabuf;
  io_vecs[1].iov_len = sizeof(extrabuf);
  // when there is enough space in this buffer, don't read into extrabuf.
  // when extrabuf is used, we read 128k-1 bytes at most.
  const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
  const ssize_t n = socket_ops::readv(fd, io_vecs, iovcnt);
  if (n < 0) {
    *saved_errno = errno;
  } else if (implicit_cast<size_t>(n) <= writable) {
    // no content in extrabuf
    mark_written(n);
  } else {
    m_write_idx = m_buffer.size();
    append(extrabuf, n - writable);
  }
  return n;
}

}  // namespace flute