#ifndef FLUTE_COMMON_DATE_H
#define FLUTE_COMMON_DATE_H

#include <flute/common/types.h>

struct tm;

namespace flute {

///
/// Date in Gregorian calendar.
///
/// This class is immutable.
/// It's recommended to pass it by value, since it's passed in register on x64.
///
class Date {
 public:
  struct YearMonthDay {
    int year;   // [1900..2500]
    int month;  // [1..12]
    int day;    // [1..31]
  };

  static const int kDaysPerWeek = 7;
  static const int kJulianDayOf1970_01_01;

  ///
  /// Constucts an invalid Date.
  ///
  Date() : m_julian_day_number(0) {}

  ///
  /// Constucts a yyyy-mm-dd Date.
  ///
  /// 1 <= month <= 12
  Date(int year, int month, int day);

  ///
  /// Constucts a Date from Julian Day Number.
  ///
  explicit Date(int julianDayNum) : m_julian_day_number(julianDayNum) {}

  ///
  /// Constucts a Date from struct tm
  ///
  explicit Date(const struct tm&);

  // default copy/assignment/dtor are Okay

  void swap(Date& that) {
    std::swap(m_julian_day_number, that.m_julian_day_number);
  }

  bool valid() const { return m_julian_day_number > 0; }

  ///
  /// Converts to yyyy-mm-dd format.
  ///
  string toIsoString() const;

  struct YearMonthDay year_month_day() const;

  int year() const { return year_month_day().year; }

  int month() const { return year_month_day().month; }

  int day() const { return year_month_day().day; }

  // [0, 1, ..., 6] => [Sunday, Monday, ..., Saturday ]
  int week_day() const { return (m_julian_day_number + 1) % kDaysPerWeek; }

  int julian_day_number() const { return m_julian_day_number; }

 private:
  int m_julian_day_number;
};

inline bool operator<(Date x, Date y) {
  return x.julian_day_number() < y.julian_day_number();
}

inline bool operator==(Date x, Date y) {
  return x.julian_day_number() == y.julian_day_number();
}

}  // namespace flute

#endif  // FLUTE_COMMON_DATE_H
