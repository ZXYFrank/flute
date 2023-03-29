#ifndef FLUTE_COMMON_MUTEX_H
#define FLUTE_COMMON_MUTEX_H

#include <flute/common/CurrentThread.h>
#include <flute/common/noncopyable.h>
#include <flute/common/LogLine.h>

#include <assert.h>
#include <pthread.h>

// Thread safety annotations
// https://clang.llvm.org/docs/ThreadSafetyAnalysis.html

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else

#define THREAD_ANNOTATION_ATTRIBUTE__(x)  // no-op
#endif

#define CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SCOPED_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define ACQUIRED_BEFORE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define ACQUIRED_AFTER(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define REQUIRES(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define REQUIRES_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define ACQUIRE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define RELEASE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define RELEASE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define TRY_ACQUIRE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define TRY_ACQUIRE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define EXCLUDES(...) THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define ASSERT_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define RETURN_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define NO_THREAD_SAFETY_ANALYSIS \
  THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

// End of thread safety annotations

#ifdef CHECK_PTHREAD_RETURN_VALUE

#ifdef NDEBUG
__BEGIN_DECLS
extern void __assert_perror_fail(int errnum, const char* file,
                                 unsigned int line,
                                 const char* function) noexcept
    __attribute__((__noreturn__));
__END_DECLS
#endif

#define MCHECK(ret)                                               \
  ({                                                              \
    __typeof__(ret) errnum = (ret);                               \
    if (__builtin_expect(errnum != 0, 0))                         \
      __assert_perror_fail(errnum, __FILE__, __LINE__, __func__); \
  })

#else  // CHECK_PTHREAD_RETURN_VALUE

#define MCHECK(ret)                 \
  ({                                \
    __typeof__(ret) errnum = (ret); \
    assert(errnum == 0);            \
    (void)errnum;                   \
  })

#endif  // CHECK_PTHREAD_RETURN_VALUE

namespace flute {

// forward declaration
class MutexLockGuard;

// Use as data member of a class, eg.

// class Foo
// {
//  public:
//   int size() const;

//  private:
//   mutable MutexLock m_mutexlock;
//   std::vector<int> data_ GUARDED_BY(m_mutexlock);
// };

class CAPABILITY("mutex") MutexLock : noncopyable {
 public:
  MutexLock(const char* name = "Lock") : m_holder_pid(0) {
    MCHECK(pthread_mutex_init(&m_pthread_mutex, NULL));
    m_name = name;
  }

  ~MutexLock() {
    assert(m_holder_pid == 0);
    MCHECK(pthread_mutex_destroy(&m_pthread_mutex));
  }

  // must be called when locked, i.e. for assertion
  bool is_locked_by_this_thread() const {
    return m_holder_pid == CurrentThread::tid();
  }

  void assert_locked_by_this_thread() const ASSERT_CAPABILITY(this) {
    assert(is_locked_by_this_thread());
  }

  pthread_mutex_t* get_pthread_mutex_ptr() { return &m_pthread_mutex; }

 private:
  void lock() ACQUIRE() {
    MCHECK(pthread_mutex_lock(&m_pthread_mutex));
    assign_holder_process();
  }

  void unlock() RELEASE() {
    unassign_holder_process();
    MCHECK(pthread_mutex_unlock(&m_pthread_mutex));
  }

 private:
  friend class Condition;
  friend class MutexLockGuard;

  // https://blog.csdn.net/weixin_30918415/article/details/101937737
  // When MutexLock is going to expose m_pthread_mutex to other functions, the
  // m_holder may be changed. However, this action is transparent to the
  // MutexLock. UnassignGuard is to GIVE UP maintaining the holder pid, in the
  // manner of RAII. e.g. UnassignGuard us(m);
  // MCHECK(pthread_cond_wait(m_pthread_cond, m_mutexlock.get_pthead_mutex()))
  class UnassignHolderProcess : noncopyable {
   public:
    explicit UnassignHolderProcess(MutexLock& mutexlock)
        : m_mutexlock(mutexlock) {
      m_mutexlock.unassign_holder_process();
      LOG_TRACE << CurrentThread::name() << " gives up " << m_mutexlock.m_name;
    }

    ~UnassignHolderProcess() {
      LOG_TRACE << CurrentThread::name() << " reclaims " << m_mutexlock.m_name;
      m_mutexlock.assign_holder_process();
    }

   private:
    MutexLock& m_mutexlock;
  };

 public:
  void unassign_holder_process() { m_holder_pid = 0; }
  void assign_holder_process() { m_holder_pid = CurrentThread::tid(); }

 private:
  pthread_mutex_t m_pthread_mutex;
  pid_t m_holder_pid;
  const char* m_name;
};

// RAII, used in a scope
class SCOPED_CAPABILITY MutexLockGuard : noncopyable {
 public:
  explicit MutexLockGuard(MutexLock& mutex, const char* name = NULL)
      ACQUIRE(mutex)
      : m_mutexlock(mutex) {
    m_mutexlock.lock();
  }

  ~MutexLockGuard() RELEASE() { m_mutexlock.unlock(); }

 private:
  MutexLock& m_mutexlock;
};

}  // namespace flute

// Prevent misuse like:
// MutexLockGuard(m_mutexlock);
// A tempory object doesn't hold the lock for long!
#define MutexLockGuard(x) static_assert(false, "Missing guard object name")

#endif  // FLUTE_COMMON_MUTEX_H
