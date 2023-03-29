#ifndef FLUTE_COMMON_NONCOPYABLE_H
#define FLUTE_COMMON_NONCOPYABLE_H

namespace flute {

// Prohibit any copy behavior
class noncopyable {
 public:
  noncopyable(const noncopyable&) = delete;
  void operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

}  // namespace flute

#endif  // FLUTE_COMMON_NONCOPYABLE_H