

#include <flute/common/Timezone.h>
#include <flute/common/Date.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include <assert.h>
// #define _BSD_SOURCE
#include <endian.h>

#include <stdint.h>
#include <stdio.h>

namespace flute {
namespace detail {

struct Transition {
  time_t gmttime;
  time_t localtime;
  int localtime_idx;

  Transition(time_t t, time_t l, int localIdx)
      : gmttime(t), localtime(l), localtime_idx(localIdx) {}
};

struct Comp {
  bool compare_gmt;

  Comp(bool gmt) : compare_gmt(gmt) {}

  bool operator()(const Transition& lhs, const Transition& rhs) const {
    if (compare_gmt)
      return lhs.gmttime < rhs.gmttime;
    else
      return lhs.localtime < rhs.localtime;
  }

  bool equal(const Transition& lhs, const Transition& rhs) const {
    if (compare_gmt)
      return lhs.gmttime == rhs.gmttime;
    else
      return lhs.localtime == rhs.localtime;
  }
};

struct Localtime {
  time_t gmt_offset;
  bool is_dst;
  int arrb_idx;

  Localtime(time_t offset, bool dst, int arrb)
      : gmt_offset(offset), is_dst(dst), arrb_idx(arrb) {}
};

inline void fill_hms(unsigned seconds, struct tm* utc) {
  utc->tm_sec = seconds % 60;
  unsigned minutes = seconds / 60;
  utc->tm_min = minutes % 60;
  utc->tm_hour = minutes / 60;
}

}  // namespace detail
const int kSecondsPerDay = 24 * 60 * 60;
}  // namespace flute

using namespace flute;
using namespace std;

struct Timezone::TimeData {
  vector<detail::Transition> transitions;
  vector<detail::Localtime> localtimes;
  vector<string> names;
  string abbreviation;
};

namespace flute {
namespace detail {

class File {
 public:
  File(const char* file) : m_fp(::fopen(file, "rb")) {}

  ~File() {
    if (m_fp) {
      ::fclose(m_fp);
    }
  }

  bool valid() const { return m_fp; }

  string read_bytes(int n) {
    char buf[n];
    ssize_t nr = ::fread(buf, 1, n, m_fp);
    if (nr != n) throw logic_error("no enough data");
    return string(buf, n);
  }

  int32_t read_int32() {
    int32_t x = 0;
    ssize_t nr = ::fread(&x, 1, sizeof(int32_t), m_fp);
    if (nr != sizeof(int32_t)) throw logic_error("bad int32_t data");
    return be32toh(x);
  }

  uint8_t read_uint8() {
    uint8_t x = 0;
    ssize_t nr = ::fread(&x, 1, sizeof(uint8_t), m_fp);
    if (nr != sizeof(uint8_t)) throw logic_error("bad uint8_t data");
    return x;
  }

 private:
  FILE* m_fp;
};

bool read_timezone_file(const char* zonefile, struct Timezone::TimeData* data) {
  File f(zonefile);
  if (f.valid()) {
    try {
      string head = f.read_bytes(4);
      if (head != "TZif") throw logic_error("bad head");
      string version = f.read_bytes(1);
      f.read_bytes(15);

      int32_t isgmtcnt = f.read_int32();
      int32_t isstdcnt = f.read_int32();
      int32_t leapcnt = f.read_int32();
      int32_t timecnt = f.read_int32();
      int32_t typecnt = f.read_int32();
      int32_t charcnt = f.read_int32();

      vector<int32_t> trans;
      vector<int> localtimes;
      trans.reserve(timecnt);
      for (int i = 0; i < timecnt; ++i) {
        trans.push_back(f.read_int32());
      }

      for (int i = 0; i < timecnt; ++i) {
        uint8_t local = f.read_uint8();
        localtimes.push_back(local);
      }

      for (int i = 0; i < typecnt; ++i) {
        int32_t gmtoff = f.read_int32();
        uint8_t isdst = f.read_uint8();
        uint8_t abbrind = f.read_uint8();

        data->localtimes.push_back(Localtime(gmtoff, isdst, abbrind));
      }

      for (int i = 0; i < timecnt; ++i) {
        int local_idx = localtimes[i];
        time_t localtime = trans[i] + data->localtimes[local_idx].gmt_offset;
        data->transitions.push_back(Transition(trans[i], localtime, local_idx));
      }

      data->abbreviation = f.read_bytes(charcnt);

      // leapcnt
      for (int i = 0; i < leapcnt; ++i) {
        // int32_t leaptime = f.readInt32();
        // int32_t cumleap = f.readInt32();
      }
      // FIXME
      (void)isstdcnt;
      (void)isgmtcnt;
    } catch (logic_error& e) {
      fprintf(stderr, "%s\n", e.what());
    }
  }
  return true;
}

const Localtime* find_local_time(const Timezone::TimeData& data,
                                 Transition sentry, Comp comp) {
  const Localtime* local = NULL;

  if (data.transitions.empty() || comp(sentry, data.transitions.front())) {
    // FIXME: should be first non dst time zone
    local = &data.localtimes.front();
  } else {
    vector<Transition>::const_iterator transI = lower_bound(
        data.transitions.begin(), data.transitions.end(), sentry, comp);
    if (transI != data.transitions.end()) {
      if (!comp.equal(sentry, *transI)) {
        assert(transI != data.transitions.begin());
        --transI;
      }
      local = &data.localtimes[transI->localtime_idx];
    } else {
      // FIXME: use TZ-env
      local = &data.localtimes[data.transitions.back().localtime_idx];
    }
  }

  return local;
}

}  // namespace detail
}  // namespace flute

Timezone::Timezone(const char* zonefile) : m_data(new Timezone::TimeData) {
  if (!detail::read_timezone_file(zonefile, m_data.get())) {
    m_data.reset();
  }
}

Timezone::Timezone(int eastOfUtc, const char* name)
    : m_data(new Timezone::TimeData) {
  m_data->localtimes.push_back(detail::Localtime(eastOfUtc, false, 0));
  m_data->abbreviation = name;
}

struct tm Timezone::to_local_time(time_t seconds) const {
  struct tm localTime;
  mem_zero(&localTime, sizeof(localTime));
  assert(m_data != NULL);
  const TimeData& data(*m_data);

  detail::Transition sentry(seconds, 0, 0);
  const detail::Localtime* local =
      find_local_time(data, sentry, detail::Comp(true));

  if (local) {
    time_t localSeconds = seconds + local->gmt_offset;
    ::gmtime_r(&localSeconds, &localTime);
    localTime.tm_isdst = local->is_dst;
    localTime.tm_gmtoff = local->gmt_offset;
    localTime.tm_zone = &data.abbreviation[local->arrb_idx];
  }

  return localTime;
}

time_t Timezone::from_local_time(const struct tm& localTm) const {
  assert(m_data != NULL);
  const TimeData& data(*m_data);

  struct tm tmp = localTm;
  time_t seconds = ::timegm(&tmp);  // FIXME: toUtcTime
  detail::Transition sentry(0, seconds, 0);
  const detail::Localtime* local =
      find_local_time(data, sentry, detail::Comp(false));
  if (localTm.tm_isdst) {
    struct tm tryTm = to_local_time(seconds - local->gmt_offset);
    if (!tryTm.tm_isdst && tryTm.tm_hour == localTm.tm_hour &&
        tryTm.tm_min == localTm.tm_min) {
      // FIXME: HACK
      seconds -= 3600;
    }
  }
  return seconds - local->gmt_offset;
}

struct tm Timezone::to_utc_time(time_t secondsSinceEpoch, bool yday) {
  struct tm utc;
  mem_zero(&utc, sizeof(utc));
  utc.tm_zone = "GMT";
  int seconds = static_cast<int>(secondsSinceEpoch % kSecondsPerDay);
  int days = static_cast<int>(secondsSinceEpoch / kSecondsPerDay);
  if (seconds < 0) {
    seconds += kSecondsPerDay;
    --days;
  }
  detail::fill_hms(seconds, &utc);
  Date date(days + Date::kJulianDayOf1970_01_01);
  Date::YearMonthDay ymd = date.year_month_day();
  utc.tm_year = ymd.year - 1900;
  utc.tm_mon = ymd.month - 1;
  utc.tm_mday = ymd.day;
  utc.tm_wday = date.week_day();

  if (yday) {
    Date startOfYear(ymd.year, 1, 1);
    utc.tm_yday = date.julian_day_number() - startOfYear.julian_day_number();
  }
  return utc;
}

time_t Timezone::from_utc_time(const struct tm& utc) {
  return from_utc_time(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                       utc.tm_hour, utc.tm_min, utc.tm_sec);
}

time_t Timezone::from_utc_time(int year, int month, int day, int hour,
                               int minute, int seconds) {
  Date date(year, month, day);
  int secondsInDay = hour * 3600 + minute * 60 + seconds;
  time_t days = date.julian_day_number() - Date::kJulianDayOf1970_01_01;
  return days * kSecondsPerDay + secondsInDay;
}
