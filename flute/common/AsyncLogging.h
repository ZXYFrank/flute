// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef FLUTE_COMMON_ASYNCLOGGING_H
#define FLUTE_COMMON_ASYNCLOGGING_H

#include <flute/common/BlockingDeque.h>
#include <flute/common/BoundedBlockingDeque.h>
#include <flute/common/CountdownLatch.h>
#include <flute/common/FixedBuffer.h>
#include <flute/common/LogStream.h>
#include <flute/common/Mutex.h>
#include <flute/common/Thread.h>

#include <atomic>
#include <vector>

namespace flute {

class AsyncLogging : noncopyable {
 public:
  AsyncLogging(const string& basename, off_t rollSize, int flushInterval = 3);

  ~AsyncLogging() {
    if (m_is_running) {
      stop();
    }
  }

  // thread safe
  void append(const char* logline, int len);

  void start() {
    m_is_running = true;
    m_backend_thread.start();
    m_backend_latch.wait();
  }

  void stop() NO_THREAD_SAFETY_ANALYSIS {
    m_is_running = false;
    m_wakeup.notify();
    m_backend_thread.join();
  }

 private:
  void backend_thread_func();

  static const int kNumBuffers;

  typedef FixedBuffer<kLogBufferSize> Buffer;
  typedef std::vector<std::unique_ptr<Buffer>> BufferPtrVector;
  typedef BufferPtrVector::value_type BufferPtr;

  const int m_flush_interval;
  std::atomic<bool> m_is_running;
  const string m_basename;
  const off_t m_rollsize;
  Thread m_backend_thread;
  CountdownLatch m_backend_latch;
  MutexLock m_mutex;
  Condition m_wakeup GUARDED_BY(m_mutex);
  BufferPtr m_cur_buffer_ptr GUARDED_BY(m_mutex);
  BufferPtr m_reserved_buffer_ptr GUARDED_BY(m_mutex);
  BufferPtrVector m_used_buffers GUARDED_BY(m_mutex);
};

}  // namespace flute

#endif  // FLUTE_COMMON_ASYNCLOGGING_H
