#ifndef FLUTE_NET_ReactorTHREADPOOL_H
#define FLUTE_NET_ReactorTHREADPOOL_H

#include <flute/common/noncopyable.h>
#include <flute/common/types.h>

#include <functional>
#include <memory>
#include <vector>

namespace flute {

class Reactor;
class ReactorThread;

class ReactorThreadPool : noncopyable {
 public:
  typedef std::function<void(Reactor*)> ThreadInitFunctor;

  ReactorThreadPool(Reactor* baseLoop, const string& nameArg);
  ~ReactorThreadPool();
  void set_pool_size(int numThreads) { m_size_pool = numThreads; }
  void start(const ThreadInitFunctor& cb = ThreadInitFunctor());

  // valid after calling start()
  /// round-robin
  Reactor* get_next_reactor();

  /// with the same hash code, it will always return the same Reactor
  Reactor* get_reactor_for_hashcode(size_t hashCode);

  std::vector<Reactor*> get_all_reactors();

  bool has_started() const { return m_has_started; }

  const string& name() const { return m_name; }

 private:
  Reactor* m_default_reactor;
  string m_name;
  bool m_has_started;
  int m_size_pool;
  int m_next_idx;
  std::vector<std::unique_ptr<ReactorThread>> m_reactor_thread_pool;
  std::vector<Reactor*> m_reactors;
};

}  // namespace flute

#endif  // FLUTE_NET_ReactorTHREADPOOL_H
