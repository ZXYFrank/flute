
//
// This is a public header file, it must only include public header files.

#ifndef FLUTE_NET_TIMERID_H
#define FLUTE_NET_TIMERID_H

namespace flute {

class Timer;

///
/// An opaque identifier, for canceling Timer.
///
// QUESTION: Why do we need this? Why embed ID directly in Timer? And further
// what's the point of using an ID.
class TimerId {
 public:
  TimerId() : m_timer_ptr(NULL), sequence_(0) {}

  TimerId(Timer* timer, int64_t seq) : m_timer_ptr(timer), sequence_(seq) {}

  // default copy-ctor, dtor and assignment are okay

  friend class TimerQueue;

 private:
  //  TODO: unique_ptr
  Timer* m_timer_ptr;
  int64_t sequence_;
};

}  // namespace flute

#endif  // FLUTE_NET_TIMERID_H
