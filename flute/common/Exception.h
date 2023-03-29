#ifndef FLUTE_COMMON_EXCEPTION_H
#define FLUTE_COMMON_EXCEPTION_H

#include <flute/common/types.h>
#include <exception>

namespace flute {

class Exception : public std::exception {
 public:
  Exception(string what);
  ~Exception() noexcept override = default;

  // default copy-ctor and operator= are okay.
  const char* what() const noexcept override { return m_message_str.c_str(); }

  const char* stackTrace() const noexcept { return m_stack_str.c_str(); }

 private:
  string m_message_str;
  string m_stack_str;
};

}  // namespace flute

#endif  // FLUTE_COMMON_EXCEPTION_H
