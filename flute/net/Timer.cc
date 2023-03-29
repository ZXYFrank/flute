

#include <flute/net/Timer.h>

using namespace flute;


AtomicInt64 Timer::g_global_timer_id;

void Timer::restart(Timestamp now)
{
  if (m_is_repeated)
  {
    m_time_expire = add_second(now, m_interval);
  }
  else
  {
    m_time_expire = Timestamp::invalid();
  }
}
