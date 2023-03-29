

#ifndef FLUTE_COMMON_ATOMIC_H
#define FLUTE_COMMON_ATOMIC_H

#include <flute/common/noncopyable.h>

#include <stdint.h>

namespace flute {

namespace detail {
template <typename T>
class AtomicIntegerT : noncopyable {
 public:
  AtomicIntegerT() : value_(0) {}

  // uncomment if you need copying and assignment
  //
  // AtomicIntegerT(const AtomicIntegerT& that)
  //   : value_(that.get())
  // {}
  //
  // AtomicIntegerT& operator=(const AtomicIntegerT& that)
  // {
  //   get_and_set(that.get());
  //   return *this;
  // }

  T get() {
    // in gcc >= 4.7: __atomic_load_n(&value_, __ATOMIC_SEQ_CST)
    return __sync_val_compare_and_swap(&value_, 0, 0);
  }

  // get the value befor adding
  T get_and_add(T x) {
    // in gcc >= 4.7: __atomic_fetch_add(&value_, x, __ATOMIC_SEQ_CST)
    return __sync_fetch_and_add(&value_, x);
  }

  // get the value after adding
  T add_and_get(T x) { return get_and_add(x) + x; }

  T increment_and_get() { return add_and_get(1); }

  T decrement_and_get() { return add_and_get(-1); }

  void add(T x) { get_and_add(x); }

  void increment() { increment_and_get(); }

  void decrement() { decrement_and_get(); }

  T get_and_set(T newValue) {
    // in gcc >= 4.7: __atomic_exchange_n(&value, newValue, __ATOMIC_SEQ_CST)
    return __sync_lock_test_and_set(&value_, newValue);
  }

 private:
  volatile T value_;
};
}  // namespace detail

typedef detail::AtomicIntegerT<int32_t> AtomicInt32;
typedef detail::AtomicIntegerT<int64_t> AtomicInt64;

}  // namespace flute

#endif  // FLUTE_COMMON_ATOMIC_H
