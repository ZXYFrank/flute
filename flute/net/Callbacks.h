
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_CALLBACKS_H
#define FLUTE_NET_CALLBACKS_H

#include <flute/common/Timestamp.h>

#include <functional>
#include <memory>

namespace flute {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

// should really belong to base/types.h, but <memory> is not included there.

template <typename T>
inline T* get_pointer(const std::shared_ptr<T>& ptr) {
  return ptr.get();
}

template <typename T>
inline T* get_pointer(const std::unique_ptr<T>& ptr) {
  return ptr.get();
}

// Adapted from google-protobuf stubs/common.h
// see License in flute/common/types.h
template <typename To, typename From>
inline ::std::shared_ptr<To> down_pointer_cast(
    const ::std::shared_ptr<From>& f) {
  if (false) {
    implicit_cast<From*, To*>(0);
  }

#ifndef NDEBUG
  assert(f == NULL || dynamic_cast<To*>(get_pointer(f)) != NULL);
#endif
  return ::std::static_pointer_cast<To>(f);
}

// All client visible callbacks go here.

class Buffer;
class TcpConnection;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
typedef std::function<void()> TimerCallback;
typedef std::function<void(const TcpConnectionPtr&)> ConnectionCallback;
typedef std::function<void(const TcpConnectionPtr&)> CloseCallback;
typedef std::function<void(const TcpConnectionPtr&)> WriteCompleteCallback;
typedef std::function<void(const TcpConnectionPtr&, size_t)>
    HighWaterMarkCallback;

// the data has been read to (buf, len)
typedef std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)>
    MessageCallback;

void dummy_conn_callback(const TcpConnectionPtr& conn);

void dummy_message_callback(const TcpConnectionPtr& conn, Buffer* buffer,
                            Timestamp receiveTime);

}  // namespace flute

#endif  // FLUTE_NET_CALLBACKS_H
