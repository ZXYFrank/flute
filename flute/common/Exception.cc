#include <flute/common/Exception.h>
#include <flute/common/CurrentThread.h>

namespace flute {

Exception::Exception(string msg)
    : m_message_str(std::move(msg)),
      m_stack_str(CurrentThread::stacktrace(false)) {}

}  // namespace flute
