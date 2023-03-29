#include <errno.h>
#include <flute/common/LogLine.h>
#include <flute/common/WeakCallback.h>
#include <flute/net/Channel.h>
#include <flute/net/Reactor.h>
#include <flute/net/Socket.h>
#include <flute/net/SocketsOps.h>
#include <flute/net/TcpConnection.h>

namespace flute {

// do nothing but set the conn state.
void dummy_conn_callback(const TcpConnectionPtr& conn) {
  LOG_TRACE << conn->local_address().to_ip_port() << " -> "
            << conn->peer_address().to_ip_port() << " is "
            << (conn->connected() ? "UP" : "DOWN");
  // do not call conn->force_close(), because some users want to register
  // message callback only.
}

// do nothing but abandon the output buffer, pretending to have send the data
// lol.
void dummy_message_callback(const TcpConnectionPtr&, Buffer* buf, Timestamp) {
  buf->retrieve_all();
}

TcpConnection::TcpConnection(Reactor* reactor, const string& name_arg,
                             int sockfd, const InetAddress& local_addr,
                             const InetAddress& peer_addr)
    : m_reactor(CHECK_NOTNULL(reactor)),
      m_name(name_arg),
      m_conn_state(kConnecting),
      m_sending_state(kNotSending),
      m_is_reading(true),
      m_socket(new Socket(sockfd)),
      m_channel(new Channel(reactor, sockfd)),
      m_local_addr(local_addr),
      m_peer_addr(peer_addr),
      m_highwater_mark(64 * 1024 * 1024) {
  m_channel->set_read_callback(
      std::bind(&TcpConnection::handle_socket_readable, this, _1));
  m_channel->set_write_callback(
      std::bind(&TcpConnection::handle_socket_writable, this));
  m_channel->set_close_callback(std::bind(&TcpConnection::handle_close, this));
  m_channel->set_error_callback(std::bind(&TcpConnection::handle_error, this));
  LOG_DEBUG << "TcpConnection::ctor[" << m_name << "] at "
            << CurrentThread::name() << " fd=" << sockfd;
  m_socket->set_keep_alive(true);
}

TcpConnection::~TcpConnection() {
  LOG_DEBUG << "TcpConnection::dtor[" << m_name << "] at "
            << CurrentThread::name() << " fd=" << m_channel->fd()
            << " state=" << state_to_string();
  assert(m_conn_state == kDisconnected);
}

bool TcpConnection::get_tcp_info(struct tcp_info* tcpi) const {
  return m_socket->get_tcp_info(tcpi);
}

string TcpConnection::get_tcp_info_string() const {
  char buf[1024];
  buf[0] = '\0';
  m_socket->get_tcp_info_string(buf, sizeof buf);
  return buf;
}

void TcpConnection::send(const void* data, int len) {
  send_string_piece(StringPiece(static_cast<const char*>(data), len));
}

void TcpConnection::send_string_piece(const StringPiece& message) {
  if (m_conn_state == kConnected) {
    if (m_reactor->is_in_reactor_thread()) {
      send_in_reactor(message);
    } else {
      void (TcpConnection::*fp)(const StringPiece& message) =
          &TcpConnection::send_in_reactor;
      m_reactor->run_asap_in_reactor(std::bind(fp,
                                               this,  // FIXME
                                               message.as_string()));
      // std::forward<string>(message)));
    }
  }
}

// FIXME efficiency!!!
void TcpConnection::send_buffer(Buffer* buf) {
  if (m_conn_state == kConnected) {
    if (m_reactor->is_in_reactor_thread()) {
      send_bytes_in_reactor(buf->peek_base(), buf->content_bytes_len());
      buf->retrieve_all();
    } else {
      void (TcpConnection::*fp)(const StringPiece& message) =
          &TcpConnection::send_in_reactor;
      // CAUTION: value copy. The string to send is stored in the std::bind
      // wrapper
      m_reactor->run_asap_in_reactor(std::bind(fp,
                                               this,  // FIXME
                                               buf->readout_all_as_string()));
      // std::forward<string>(message)));
    }
  }
}

void TcpConnection::send_in_reactor(const StringPiece& message) {
  send_bytes_in_reactor(message.data(), message.size());
}

// TCP is byte-stream oriented.
void TcpConnection::send_bytes_in_reactor(const void* data,
                                          size_t total_num_bytes) {
  m_reactor->assert_in_reactor_thread();
  ssize_t nwrote = 0;
  size_t remaining_num_bytes = total_num_bytes;
  bool fault_error = false;
  if (m_conn_state == kDisconnected) {
    LOG_WARN << "disconnected, give up writing";
    return;
  }

  // // NOTE: You can enable this first try to save some time, if the task do
  // not have to queue

  // // ================try writing at entry================
  // // if nothing in output queue and nothing waiting in the buffer, try
  // writing
  // // directly
  // if (!m_channel->is_writing() && m_output_buffer.content_bytes_len() == 0) {
  //   nwrote = socket_ops::write(m_channel->fd(), data, total_num_bytes);
  //   if (nwrote >= 0) {
  //     remaining_num_bytes = total_num_bytes - nwrote;
  //     // data has been written to OS socket buffer completely.
  //     if (remaining_num_bytes == 0 && m_write_complete_callback) {
  //       // notify the reactor
  //       m_reactor->queue_in_reactor(
  //           std::bind(m_write_complete_callback, shared_from_this()));
  //     }
  //   } else {  // nwrote < 0
  //     nwrote = 0;
  //     if (errno != EWOULDBLOCK) {
  //       LOG_SYSERR << m_name << "TcpConnection::send_in_reactor";
  //       if (errno == EPIPE || errno == ECONNRESET)  // FIXME: any others?
  //       {
  //         fault_error = true;
  //       }
  //     }
  //   }
  // }
  // // ================try writing at entry================

  assert(remaining_num_bytes <= total_num_bytes);
  // writing has not completed yet.
  if (!fault_error && remaining_num_bytes > 0) {
    size_t old_len = m_output_buffer.content_bytes_len();
    // FIXME: unused high water mark
    if (old_len + remaining_num_bytes >= m_highwater_mark &&
        old_len < m_highwater_mark && m_high_watermark_callback) {
      m_reactor->queue_in_reactor(std::bind(m_high_watermark_callback,
                                            shared_from_this(),
                                            old_len + remaining_num_bytes));
    }
    LOG_TRACE << m_name << " has " << remaining_num_bytes
              << " remaining bytes to write";
    m_output_buffer.append(static_cast<const char*>(data) + nwrote,
                           remaining_num_bytes);
    if (!m_channel->is_writing()) {
      // NOTE: notify the reactor and the poller that the channel still has sth
      // to write. Thus the remaining bytes in m_output_buffer is going to be
      // sent.
      // See TcpConnection::handle_write()
      m_channel->want_to_write();
    }
  }
}

// register and submit the zero copy task. The work will be done by submitting
// multiple subtasks
// CAUTION: You should not write anything into the socket other than this, to
// maintain the sequentiality of the file data.
void TcpConnection::enable_zero_copy(const ZeroCopierPtr& zero_copier_ptr) {
  m_zero_copier_ptr = zero_copier_ptr;
  m_zero_copier_ptr->set_target_fd(m_channel->fd());
  m_zero_copier_ptr->start();
  m_channel->want_to_write();
}

void TcpConnection::shutdown() {
  // FIXME: use compare and swap
  if (m_conn_state == kConnected) {
    set_state(kDisconnecting);
    // FIXME: shared_from_this()?
    m_reactor->run_asap_in_reactor(
        std::bind(&TcpConnection::shutdown_in_reactor, this));
  }
}

void TcpConnection::shutdown_in_reactor() {
  m_reactor->assert_in_reactor_thread();
  if (!m_channel->is_writing()) {
    m_socket->shutdown_write();
    LOG_TRACE << "TCP conn " << m_name << " shutdown writing";
  }
}

void TcpConnection::shutdown_and_force_close_after(double seconds) {
  // FIXME: use compare and swap
  if (m_conn_state == kConnected) {
    set_state(kDisconnecting);
    m_reactor->run_asap_in_reactor(
        std::bind(&TcpConnection::shutdown_and_force_close_in_reactor_after,
                  this, seconds));
  }
}

void TcpConnection::shutdown_and_force_close_in_reactor_after(double seconds) {
  m_reactor->assert_in_reactor_thread();
  if (!m_channel->is_writing()) {
    // we are not writing
    m_socket->shutdown_write();
  }
  LOG_TRACE << "TCP conn " << m_name << " shutdown_and_force_close";
  m_reactor->run_after(
      seconds, make_weak_callback(shared_from_this(),
                                  &TcpConnection::force_close_in_reactor));
}

void TcpConnection::force_close() {
  // FIXME: use compare and swap
  if (m_conn_state == kConnected || m_conn_state == kDisconnecting) {
    set_state(kDisconnecting);
    m_reactor->queue_in_reactor(
        std::bind(&TcpConnection::force_close_in_reactor, shared_from_this()));
  }
}

void TcpConnection::force_close_with_delay(double seconds) {
  if (m_conn_state == kConnected || m_conn_state == kDisconnecting) {
    set_state(kDisconnecting);
    m_reactor->run_after(
        seconds,
        make_weak_callback(
            shared_from_this(),
            &TcpConnection::force_close));  // not force_close_in_reactor to
                                            // avoid race condition
  }
}

void TcpConnection::force_close_in_reactor() {
  m_reactor->assert_in_reactor_thread();
  if (m_conn_state == kConnected || m_conn_state == kDisconnecting) {
    // as if we received 0 byte in handle_read();
    handle_close();
  }
}

const char* TcpConnection::state_to_string() const {
  switch (m_conn_state) {
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    case kDisconnected:
      return "kDisconnected";
    default:
      return "unknown state";
  }
}

void TcpConnection::set_tcp_nodelay(bool on) { m_socket->set_tcp_nodelay(on); }

void TcpConnection::start_read() {
  m_reactor->run_asap_in_reactor(
      std::bind(&TcpConnection::start_read_in_reactor, this));
}

void TcpConnection::start_read_in_reactor() {
  m_reactor->assert_in_reactor_thread();
  if (!m_is_reading || !m_channel->is_reading()) {
    m_channel->wang_to_read();
    m_is_reading = true;
  }
}

void TcpConnection::stop_read() {
  m_reactor->run_asap_in_reactor(
      std::bind(&TcpConnection::stop_read_in_reactor, this));
}

void TcpConnection::stop_read_in_reactor() {
  m_reactor->assert_in_reactor_thread();
  if (m_is_reading || m_channel->is_reading()) {
    m_channel->end_reading();
    m_is_reading = false;
  }
}

void TcpConnection::connect_established() {
  m_reactor->assert_in_reactor_thread();
  assert(m_conn_state == kConnecting);
  set_state(kConnected);
  m_channel->tie(shared_from_this());
  m_channel->wang_to_read();

  m_conn_callback(shared_from_this());
}

void TcpConnection::connect_destroyed() {
  m_reactor->assert_in_reactor_thread();
  if (m_conn_state == kConnected) {
    set_state(kDisconnected);
    m_channel->end_all();
    m_conn_callback(shared_from_this());
  }
  m_channel->remove_self_from_reactor();
}

void TcpConnection::handle_socket_readable(Timestamp receiveTime) {
  m_reactor->assert_in_reactor_thread();
  int saved_errno = 0;
  ssize_t n = m_input_buffer.read_all_from(m_channel->fd(), &saved_errno);
  // QUESTION: what if n < bytes received.
  if (n > 0) {
    m_message_callback(shared_from_this(), &m_input_buffer, receiveTime);
  } else if (n == 0) {
    LOG_INFO << m_name << " READ 0 bytes: FIN received";
    handle_close();
  } else {
    errno = saved_errno;
    LOG_SYSERR << m_name << "TcpConnection::handle_read";
    handle_error();
  }
}

enum WriteFlags {
  kNothingInBuffer,
  kBufferAllWritten,
  kBufferPartialWritten,
  kSocketWriteError
};

// The assertion(in reactor thread, i.e. IO thread) will guarantee that the
// function will not execute sequentially.
// FIXME: Note that we are always trying to consume the whole buffer first.
// However, the buffer, which holds all the content we need to write, may be
// modified by other threads, in a async manner.
// So now, the function only works with the buffer not being modified ever since
// we start to write from it.
// FIXME: If we keep appending content to the buffer, ITERATORS may FAIL.
// TODO: Whether reenterable?
// TODO: IO queue & one-task-one-buffer.
// task = m_io_queue.front()
// task.do()
// if (task.has_finished()) {m_task_queue.pop_front();}
void TcpConnection::handle_socket_writable() {
  m_reactor->assert_in_reactor_thread();
  if (m_channel->is_writing()) {
    if (m_sending_state == kNotSending || m_sending_state == kSendingBuffer) {
      m_sending_state = kSendingBuffer;
      write_socket_from_buffer();
      transfer_sending_state();
    }
    // if the buffer has nothing to write, we do copy works
    if (m_output_buffer.content_bytes_len() == 0 &&
        m_zero_copier_ptr != nullptr &&
        (m_sending_state == kNotSending || m_sending_state == kSendingFile)) {
      m_sending_state = kSendingFile;
      m_zero_copier_ptr->send_one_chunk();
      transfer_sending_state();
      if (m_zero_copier_ptr->has_finished()) {
        m_zero_copier_ptr.reset();
      }
    }
    if (m_sending_state == kNotSending) {
      LOG_INFO << "TCPConn" << m_name << " writing finished.";
      m_channel->end_writing();
      if (m_write_complete_callback) {
        m_reactor->queue_in_reactor(
            std::bind(m_write_complete_callback, shared_from_this()));
      }
      // NOTE: Finish current writing task. m_state is just a mark
      if (m_conn_state == kDisconnecting) {
        shutdown_in_reactor();
      }
    }
  } else {
    LOG_TRACE << m_name << "Connection fd = " << m_channel->fd()
              << " is not considering writing";
  }
}

void TcpConnection::transfer_sending_state() {
  if (m_sending_state == kNotSending) {
    if (m_output_buffer.content_bytes_len() != 0) {
      m_sending_state = kSendingBuffer;
    } else if (m_zero_copier_ptr != nullptr &&
               !m_zero_copier_ptr->has_finished()) {
      m_sending_state = kSendingFile;
    }
  } else if (m_sending_state == kSendingBuffer) {
    if (m_output_buffer.content_bytes_len() == 0) {
      m_sending_state = kNotSending;
    }
  } else if (m_sending_state == kSendingFile) {
    if (m_zero_copier_ptr->has_finished()) {
      m_sending_state = kNotSending;
    }
  }
}
void TcpConnection::write_socket_from_buffer() {
  if (m_output_buffer.content_bytes_len() == 0) {
    return;
  }
  ssize_t n = socket_ops::write(m_channel->fd(), m_output_buffer.peek_base(),
                                m_output_buffer.content_bytes_len());

  if (n < 0) {
    LOG_SYSERR << m_name << "TcpConnection::handle_write";
  } else {
    m_output_buffer.retrieve(n);
  }
}

void TcpConnection::handle_close() {
  m_reactor->assert_in_reactor_thread();
  LOG_TRACE << m_name << " handling close, state = " << state_to_string();
  assert(m_conn_state == kConnected || m_conn_state == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  set_state(kDisconnected);
  m_channel->end_all();

  TcpConnectionPtr guard_this(shared_from_this());
  m_conn_callback(guard_this);
  // must be the last line
  // ONGOING: When will server close a conn?
  m_close_callback(guard_this);
  LOG_TRACE << m_name << "close_callback finished";
}

void TcpConnection::handle_error() {
  int err = socket_ops::getSocketError(m_channel->fd());
  LOG_ERROR << "TcpConnection::handle_error [" << m_name
            << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

}  // namespace flute