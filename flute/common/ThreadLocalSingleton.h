// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef FLUTE_COMMON_THREADLOCALSINGLETON_H
#define FLUTE_COMMON_THREADLOCALSINGLETON_H

#include <flute/common/noncopyable.h>

#include <assert.h>
#include <pthread.h>

namespace flute {

template <typename T>
class ThreadLocalSingleton : noncopyable {
 public:
  ThreadLocalSingleton() = delete;
  ~ThreadLocalSingleton() = delete;

  static T& instance() {
    if (!t_instance) {
      t_instance = new T();
      m_deleter.set(t_instance);
    }
    return *t_instance;
  }

  static T* pointer() { return t_instance; }

 private:
  static void destructor(void* obj) {
    assert(obj == t_instance);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy;
    (void)dummy;
    delete t_instance;
    t_instance = 0;
  }

  class Deleter {
   public:
    Deleter() {
      pthread_key_create(&m_pkey, &ThreadLocalSingleton::destructor);
    }

    ~Deleter() { pthread_key_delete(m_pkey); }

    void set(T* new_obj) {
      assert(pthread_getspecific(m_pkey) == NULL);
      pthread_setspecific(m_pkey, new_obj);
    }

    pthread_key_t m_pkey;
  };

  static __thread T* t_instance;
  static Deleter m_deleter;
};

template <typename T>
__thread T* ThreadLocalSingleton<T>::t_instance = 0;

template <typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::m_deleter;

}  // namespace flute
#endif  // FLUTE_COMMON_THREADLOCALSINGLETON_H
