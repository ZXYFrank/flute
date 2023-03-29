
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_ReactorTHREAD_H
#define FLUTE_NET_ReactorTHREAD_H

#include <flute/common/Condition.h>
#include <flute/common/Mutex.h>
#include <flute/common/Thread.h>

namespace flute {

class Reactor;

class ReactorThread : noncopyable {
 public:
  typedef std::function<void(Reactor*)> ThreadInitFunctor;

  ReactorThread(const ThreadInitFunctor& cb = ThreadInitFunctor(),
                const string& name = string());
  ~ReactorThread();
  Reactor* start_reactor();

 private:
  void reactor_thread_loop_func();

  Reactor* m_reactor GUARDED_BY(m_mutexlock);
  bool m_is_exiting;
  Thread m_thread;
  MutexLock m_mutexlock;
  Condition m_cond GUARDED_BY(m_mutexlock);
  ThreadInitFunctor m_reactor_init_func;
};

}  // namespace flute

#endif  // FLUTE_NET_ReactorTHREAD_H
