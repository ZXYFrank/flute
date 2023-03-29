// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef FLUTE_COMMON_THREADLOCAL_H
#define FLUTE_COMMON_THREADLOCAL_H

#include <flute/common/Mutex.h>  // MCHECK
#include <flute/common/noncopyable.h>

#include <pthread.h>

namespace flute {

template <typename T>
class ThreadLocal : noncopyable {
 public:
  ThreadLocal() {
    MCHECK(pthread_key_create(&m_pkey, &ThreadLocal::destructor));
  }

  ~ThreadLocal() { MCHECK(pthread_key_delete(m_pkey)); }

  T& value() {
    T* thread_instance = static_cast<T*>(pthread_getspecific(m_pkey));
    if (!thread_instance) {
      T* new_obj = new T();
      MCHECK(pthread_setspecific(m_pkey, new_obj));
      thread_instance = new_obj;
    }
    return *thread_instance;
  }

 private:
  static void destructor(void* x) {
    T* obj = static_cast<T*>(x);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy;
    (void)dummy;
    delete obj;
  }

 private:
  pthread_key_t m_pkey;
};

}  // namespace flute

#endif  // FLUTE_COMMON_THREADLOCAL_H
