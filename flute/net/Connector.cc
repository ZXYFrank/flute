
//

#include <flute/net/Connector.h>

#include <flute/common/LogLine.h>
#include <flute/net/Channel.h>
#include <flute/net/Reactor.h>
#include <flute/net/SocketsOps.h>

#include <errno.h>

using namespace flute;


const int Connector::kMaxRetryDelayMs;

Connector::Connector(Reactor* reactor, const InetAddress& serverAddr)
  : m_reactor(reactor),
    serverAddr_(serverAddr),
    m_is_connected(false),
    m_state(kDisconnected),
    m_retry_delay_ms(kInitRetryDelayMs)
{
  LOG_DEBUG << "ctor[" << CurrentThread::name() << "]";
}

Connector::~Connector()
{
  LOG_DEBUG << "dtor[" << CurrentThread::name() << "]";
  assert(!m_channel);
}

void Connector::start()
{
  m_is_connected = true;
  m_reactor->run_asap_in_reactor(std::bind(&Connector::start_in_reactor, this)); // FIXME: unsafe
}

void Connector::start_in_reactor()
{
  m_reactor->assert_in_reactor_thread();
  assert(m_state == kDisconnected);
  if (m_is_connected)
  {
    connect();
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

void Connector::stop()
{
  m_is_connected = false;
  m_reactor->queue_in_reactor(std::bind(&Connector::stop_in_reactor, this)); // FIXME: unsafe
  // FIXME: remove timer
}

void Connector::stop_in_reactor()
{
  m_reactor->assert_in_reactor_thread();
  if (m_state == kConnecting)
  {
    set_state(kDisconnected);
    int sockfd = remove_and_reset_channel();
    retry(sockfd);
  }
}

void Connector::connect()
{
  int sockfd = socket_ops::create_sockfd_nonblocking_or_abort(serverAddr_.family());
  int ret = socket_ops::connect(sockfd, serverAddr_.get_sock_addr());
  int savedErrno = (ret == 0) ? 0 : errno;
  switch (savedErrno)
  {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      connecting(sockfd);
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd);
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_SYSERR << "connect error in Connector::start_in_reactor " << savedErrno;
      socket_ops::close(sockfd);
      break;

    default:
      LOG_SYSERR << "Unexpected error in Connector::start_in_reactor " << savedErrno;
      socket_ops::close(sockfd);
      // connectErrorCallback_();
      break;
  }
}

void Connector::restart()
{
  m_reactor->assert_in_reactor_thread();
  set_state(kDisconnected);
  m_retry_delay_ms = kInitRetryDelayMs;
  m_is_connected = true;
  start_in_reactor();
}

void Connector::connecting(int sockfd)
{
  set_state(kConnecting);
  assert(!m_channel);
  m_channel.reset(new Channel(m_reactor, sockfd));
  m_channel->set_write_callback(
      std::bind(&Connector::handle_write, this)); // FIXME: unsafe
  m_channel->set_error_callback(
      std::bind(&Connector::handle_error, this)); // FIXME: unsafe

  // channel_->tie(shared_from_this()); is not working,
  // as channel_ is not managed by shared_ptr
  m_channel->want_to_write();
}

int Connector::remove_and_reset_channel()
{
  m_channel->end_all();
  m_channel->remove_self_from_reactor();
  int sockfd = m_channel->fd();
  // Can't reset channel_ here, because we are inside Channel::handleEvent
  m_reactor->queue_in_reactor(std::bind(&Connector::reset_channel, this)); // FIXME: unsafe
  return sockfd;
}

void Connector::reset_channel()
{
  m_channel.reset();
}

void Connector::handle_write()
{
  LOG_TRACE << "Connector::handle_write " << m_state;

  if (m_state == kConnecting)
  {
    int sockfd = remove_and_reset_channel();
    int err = socket_ops::getSocketError(sockfd);
    if (err)
    {
      LOG_WARN << "Connector::handle_write - SO_ERROR = "
               << err << " " << flute::strerror_tl(err);
      retry(sockfd);
    }
    else if (socket_ops::isSelfConnect(sockfd))
    {
      LOG_WARN << "Connector::handle_write - Self connect";
      retry(sockfd);
    }
    else
    {
      set_state(kConnected);
      if (m_is_connected)
      {
        m_new_conn_callback(sockfd);
      }
      else
      {
        socket_ops::close(sockfd);
      }
    }
  }
  else
  {
    // what happened?
    assert(m_state == kDisconnected);
  }
}

void Connector::handle_error()
{
  LOG_ERROR << "Connector::handle_error state=" << m_state;
  if (m_state == kConnecting)
  {
    int sockfd = remove_and_reset_channel();
    int err = socket_ops::getSocketError(sockfd);
    LOG_TRACE << "SO_ERROR = " << err << " " << flute::strerror_tl(err);
    retry(sockfd);
  }
}

void Connector::retry(int sockfd)
{
  socket_ops::close(sockfd);
  set_state(kDisconnected);
  if (m_is_connected)
  {
    LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.to_ip_port()
             << " in " << m_retry_delay_ms << " milliseconds. ";
    m_reactor->run_after(m_retry_delay_ms/1000.0,
                    std::bind(&Connector::start_in_reactor, shared_from_this()));
    m_retry_delay_ms = std::min(m_retry_delay_ms * 2, kMaxRetryDelayMs);
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

