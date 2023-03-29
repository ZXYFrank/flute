#include <flute/common/AsyncLogging.h>
#include <flute/common/LogFile.h>
#include <flute/common/Timestamp.h>
#include <stdio.h>

namespace flute {

AsyncLogging::AsyncLogging(const string& basename, off_t rollsize,
                           int flushInterval)
    : m_flush_interval(flushInterval),
      m_is_running(false),
      m_basename(basename),
      m_rollsize(rollsize),
      m_backend_thread(std::bind(&AsyncLogging::backend_thread_func, this),
                       "AsyncLogging"),
      m_backend_latch(1),
      m_mutex(),
      m_wakeup(m_mutex),
      m_cur_buffer_ptr(new Buffer),
      m_reserved_buffer_ptr(new Buffer),
      m_used_buffers() {
  m_cur_buffer_ptr->reset_buffer();
  m_reserved_buffer_ptr->reset_buffer();
  m_used_buffers.reserve(kNumBuffers);
}

void AsyncLogging::append(const char* logline, int len) {
  flute::MutexLockGuard lock(m_mutex);
  if (m_cur_buffer_ptr->avail() > len) {
    m_cur_buffer_ptr->append(logline, len);
  } else {
    // current buffer need to be archived
    m_used_buffers.push_back(std::move(m_cur_buffer_ptr));

    if (m_reserved_buffer_ptr) {
      // we can directly use the reserved buffer instead of creating one
      // ourselves. Actually, there could be more than one reserved buffers.
      // That's the space-time trade-off.
      m_cur_buffer_ptr = std::move(m_reserved_buffer_ptr);
    } else {
      // Or we should create a new buffer
      m_cur_buffer_ptr.reset(new Buffer);
    }
    // Then we are always writting to the buffer that we call "current"
    m_cur_buffer_ptr->append(logline, len);
    m_wakeup.notify();
  }
}

void AsyncLogging::backend_thread_func() {
  assert(m_is_running == true);
  m_backend_latch.countdown();
  LogFile output(m_basename, m_rollsize, false);
  BufferPtr new_buffer_ptr1(new Buffer);
  BufferPtr new_buffer_ptr2(new Buffer);
  new_buffer_ptr1->reset_buffer();
  new_buffer_ptr2->reset_buffer();
  BufferPtrVector buffers_to_write;
  buffers_to_write.reserve(kNumBuffers);

  while (m_is_running) {
    assert(new_buffer_ptr1 && new_buffer_ptr1->length() == 0);
    assert(new_buffer_ptr2 && new_buffer_ptr2->length() == 0);
    assert(buffers_to_write.empty());

    {
      flute::MutexLockGuard lock(m_mutex);
      if (m_used_buffers.empty())  // unusual usage!
      {
        // continue even if m_buffers is empty
        m_wakeup.wait_for_seconds(m_flush_interval);
      }
      m_used_buffers.push_back(std::move(m_cur_buffer_ptr));
      // std::move will set the ptr to empty
      m_cur_buffer_ptr = std::move(new_buffer_ptr1);
      // m_wriiten buffers are swapped to empty
      buffers_to_write.swap(m_used_buffers);
      if (!m_reserved_buffer_ptr) {
        m_reserved_buffer_ptr = std::move(new_buffer_ptr2);
      }
    }

    assert(!buffers_to_write.empty());

    if (buffers_to_write.size() > 2) {
      char buf[256];
      snprintf(buf, sizeof buf,
               "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().to_formatted_string().c_str(),
               buffers_to_write.size() - 2);
      fputs(buf, stderr);
      output.append(buf, static_cast<int>(strlen(buf)));
      // abondon the extra buffers
      buffers_to_write.erase(buffers_to_write.begin() + 2,
                             buffers_to_write.end());
    }

    for (const auto& buffer : buffers_to_write) {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffer->data(), buffer->length());
    }

    if (buffers_to_write.size() > 2) {
      // drop non-reset_buffer-ed buffers, avoid trashing
      buffers_to_write.resize(2);
    }

    // In fact new_buffer_ptr1 is always empty. We have move it to the
    if (!new_buffer_ptr1) {
      assert(!buffers_to_write.empty());
      new_buffer_ptr1 = std::move(buffers_to_write.back());
      buffers_to_write.pop_back();
      new_buffer_ptr1->reset_cur();
    }

    if (!new_buffer_ptr2) {
      assert(!buffers_to_write.empty());
      new_buffer_ptr2 = std::move(buffers_to_write.back());
      buffers_to_write.pop_back();
      new_buffer_ptr2->reset_cur();
    }
    buffers_to_write.clear();
    output.flush();
  }
  output.flush();
}

}  // namespace flute