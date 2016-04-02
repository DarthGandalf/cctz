// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <type_traits>

// Disable constexpr support unless we are using clang in C++14 mode.
#if __clang__ && __cpp_constexpr >= 201304
#define CONSTEXPR constexpr
#else
#define CONSTEXPR
#endif

namespace cctz {
namespace detail {

// Normalized civil-time fields: Y-M-D HH:MM:SS.
struct fields {
  int y;
  int m;
  int d;
  int hh;
  int mm;
  int ss;
};

struct second_tag {};
struct minute_tag : second_tag {};
struct hour_tag : minute_tag {};
struct day_tag : hour_tag {};
struct month_tag : day_tag {};
struct year_tag : month_tag {};

////////////////////////////////////////////////////////////////////////

// Field normalization.

namespace impl {

inline CONSTEXPR bool is_leap_year(int y) {
  return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}
inline CONSTEXPR int year_index(int y, int m) {
  return (((y + (m > 2)) % 400) + 400) % 400;
}
inline CONSTEXPR int days_per_century(int y, int m) {
  // The number of days in the 100 years starting in the mod-400 index year,
  // stored as a 36524-deficit value (i.e., 0 == 36524, 1 == 36525).
  CONSTEXPR const signed char k_days_per_century[400] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  };
  return 36524 + k_days_per_century[year_index(y, m)];
}
inline CONSTEXPR int days_per_4years(int y, int m) {
  // The number of days in the 4 years starting in the mod-400 index year,
  // stored as a 1460-deficit value (i.e., 0 == 1460, 1 == 1461).
  CONSTEXPR const signed char k_days_per_4years[400] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  };
  return 1460 + k_days_per_4years[year_index(y, m)];
}
inline CONSTEXPR int days_per_year(int y, int m) {
  return is_leap_year(y + (m > 2)) ? 366 : 365;
}
inline CONSTEXPR int days_per_month(int y, int m) {
  // The month lengths in non-leap years.
  CONSTEXPR const signed char k_dpm[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };
  return k_dpm[m - 1] + (m == 2 && is_leap_year(y));
}

inline CONSTEXPR fields n_day(int y, int m, int d, int c, int hh, int mm,
                              int ss) {
  y += (c / 146097) * 400;
  c %= 146097;
  if (c < 0) {
    y -= 400;
    c += 146097;
  }
  y += (d / 146097) * 400;
  d = d % 146097 + c;
  if (d <= 0) {
    y -= 400;
    d += 146097;
  }
  int n = days_per_century(y, m);
  while (d > n) {
    d -= n;
    n = days_per_century(y += 100, m);
  }
  n = days_per_4years(y, m);
  while (d > n) {
    d -= n;
    n = days_per_4years(y += 4, m);
  }
  n = days_per_year(y, m);
  while (d > n) {
    d -= n;
    n = days_per_year(++y, m);
  }
  n = days_per_month(y, m);
  while (d > n) {
    d -= n;
    n = (m == 12) ? days_per_month(++y, m = 1) : days_per_month(y, ++m);
  }
  return fields{y, m, d, hh, mm, ss};
}

inline CONSTEXPR fields n_mon(int y, int m, int d, int cd, int hh, int mm,
                              int ss) {
  y += m / 12;
  m %= 12;
  if (m <= 0) {
    y -= 1;
    m += 12;
  }
  return n_day(y, m, d, cd, hh, mm, ss);
}

inline CONSTEXPR fields n_hour(int y, int m, int d, int c, int hh, int mm,
                               int ss) {
  c += hh / 24;
  hh %= 24;
  if (hh < 0) {
    c -= 1;
    hh += 24;
  }
  return n_mon(y, m, d, c, hh, mm, ss);
}

inline CONSTEXPR fields n_min(int y, int m, int d, int hh, int c, int mm,
                              int ss) {
  c += mm / 60;
  mm %= 60;
  if (mm < 0) {
    c -= 1;
    mm += 60;
  }
  return n_hour(y, m, d, hh / 24 + c / 24, hh % 24 + c % 24, mm, ss);
}

inline CONSTEXPR fields n_sec(int y, int m, int d, int hh, int mm, int ss) {
  int c = ss / 60;
  ss %= 60;
  if (ss < 0) {
    c -= 1;
    ss += 60;
  }
  return n_min(y, m, d, hh, mm / 60 + c / 60, mm % 60 + c % 60, ss);
}

}  // namespace impl

////////////////////////////////////////////////////////////////////////

namespace impl {

// Map a (normalized) Y/M/D to the number of days before/after 1970-01-01.
// Will overflow outside of the range [-5877641-06-23 ... 5881580-07-11].
inline CONSTEXPR int doy(int m, int d) {
  return (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
}
inline CONSTEXPR int doe(int yoe, int m, int d) {
  return yoe * 365 + yoe / 4 - yoe / 100 + doy(m, d);
}
inline CONSTEXPR int era_eymd_ord(int era, int eyear, int m, int d) {
  return era * 146097 + doe(eyear - era * 400, m, d) - 719468;
}
inline CONSTEXPR int eymd_ord(int eyear, int m, int d) {
  return era_eymd_ord((eyear >= 0 ? eyear : eyear - 399) / 400, eyear, m, d);
}
inline CONSTEXPR int ymd_ord(int y, int m, int d) {
  return eymd_ord(m <= 2 ? y - 1 : y, m, d);
}

}  // namespace impl

////////////////////////////////////////////////////////////////////////

// Aligns the (normalized) fields struct to the indicated field.
inline CONSTEXPR fields align(second_tag, fields f) {
  return f;
}
inline CONSTEXPR fields align(minute_tag, fields f) {
  return fields{f.y, f.m, f.d, f.hh, f.mm, 0};
}
inline CONSTEXPR fields align(hour_tag, fields f) {
  return fields{f.y, f.m, f.d, f.hh, 0, 0};
}
inline CONSTEXPR fields align(day_tag, fields f) {
  return fields{f.y, f.m, f.d, 0, 0, 0};
}
inline CONSTEXPR fields align(month_tag, fields f) {
  return fields{f.y, f.m, 1, 0, 0, 0};
}
inline CONSTEXPR fields align(year_tag, fields f) {
  return fields{f.y, 1, 1, 0, 0, 0};
}

////////////////////////////////////////////////////////////////////////

// Increments the indicated (normalized) field by "n".
inline CONSTEXPR fields step(second_tag, fields f, int n) {
  return impl::n_sec(f.y, f.m, f.d, f.hh, f.mm + n / 60, f.ss + n % 60);
}
inline CONSTEXPR fields step(minute_tag, fields f, int n) {
  return impl::n_min(f.y, f.m, f.d, f.hh + n / 60, 0, f.mm + n % 60, f.ss);
}
inline CONSTEXPR fields step(hour_tag, fields f, int n) {
  return impl::n_hour(f.y, f.m, f.d + n / 24, 0, f.hh + n % 24, f.mm, f.ss);
}
inline CONSTEXPR fields step(day_tag, fields f, int n) {
  return impl::n_day(f.y, f.m, f.d, n, f.hh, f.mm, f.ss);
}
inline CONSTEXPR fields step(month_tag, fields f, int n) {
  return impl::n_mon(f.y + n / 12, f.m + n % 12, f.d, 0, f.hh, f.mm, f.ss);
}
inline CONSTEXPR fields step(year_tag, fields f, int n) {
  return fields{f.y + n, f.m, f.d, f.hh, f.mm, f.ss};
}

////////////////////////////////////////////////////////////////////////

// Returns the difference between fields structs using the indicated unit.
inline CONSTEXPR int difference(year_tag, fields f1, fields f2) {
  return f1.y - f2.y;
}
inline CONSTEXPR int difference(month_tag, fields f1, fields f2) {
  return difference(year_tag{}, f1, f2) * 12 + (f1.m - f2.m);
}
inline CONSTEXPR int difference(day_tag, fields f1, fields f2) {
  return impl::ymd_ord(f1.y, f1.m, f1.d) - impl::ymd_ord(f2.y, f2.m, f2.d);
}
inline CONSTEXPR int difference(hour_tag, fields f1, fields f2) {
  return difference(day_tag{}, f1, f2) * 24 + (f1.hh - f2.hh);
}
inline CONSTEXPR int difference(minute_tag, fields f1, fields f2) {
  return difference(hour_tag{}, f1, f2) * 60 + (f1.mm - f2.mm);
}
inline CONSTEXPR int difference(second_tag, fields f1, fields f2) {
  return difference(minute_tag{}, f1, f2) * 60 + (f1.ss - f2.ss);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
class civil_time {
 public:
  explicit CONSTEXPR civil_time(int y, int m = 1, int d = 1, int hh = 0,
                                int mm = 0, int ss = 0)
      : civil_time(impl::n_sec(y, m, d, hh, mm, ss)) {}

  CONSTEXPR civil_time() : civil_time(1970) {}
  CONSTEXPR civil_time(const civil_time&) = default;
  civil_time& operator=(const civil_time&) = default;

  // Conversion between civil times of different alignment. Conversion to
  // a more precise alignment is allowed implicitly (e.g., day -> hour),
  // but conversion where information is discarded must be explicit
  // (e.g., second -> minute).
  template <typename U, typename S>
  using preserves_data =
      typename std::enable_if<std::is_base_of<U, S>::value>::type;
  template <typename U>
  CONSTEXPR civil_time(const civil_time<U>& ct,
                       preserves_data<T, U>* = nullptr)
      : civil_time(ct.f_) {}
  template <typename U>
  explicit CONSTEXPR civil_time(const civil_time<U>& ct,
                                preserves_data<U, T>* = nullptr)
      : civil_time(ct.f_) {}

  // Field accessors.
  CONSTEXPR int year() const { return f_.y; }
  CONSTEXPR int month() const { return f_.m; }
  CONSTEXPR int day() const { return f_.d; }
  CONSTEXPR int hour() const { return f_.hh; }
  CONSTEXPR int minute() const { return f_.mm; }
  CONSTEXPR int second() const { return f_.ss; }

  // Assigning arithmetic.
  CONSTEXPR civil_time& operator+=(int n) {
    f_ = step(T{}, f_, n);
    return *this;
  }
  CONSTEXPR civil_time& operator-=(int n) {
    if (n != std::numeric_limits<int>::min()) {
      f_ = step(T{}, f_, -n);
    } else {
      f_ = step(T(), step(T{}, f_, -(n + 1)), 1);
    }
    return *this;
  }
  CONSTEXPR civil_time& operator++() {
    return *this += 1;
  }
  CONSTEXPR civil_time operator++(int) {
    civil_time a = *this;
    ++*this;
    return a;
  }
  CONSTEXPR civil_time& operator--() {
    return *this -= 1;
  }
  CONSTEXPR civil_time operator--(int) {
    civil_time a = *this;
    --*this;
    return a;
  }

  // Binary arithmetic operators.
  inline friend CONSTEXPR civil_time operator+(const civil_time& a, int n) {
    return civil_time(step(T{}, a.f_, n));
  }
  inline friend CONSTEXPR civil_time operator+(int n, const civil_time& a) {
    return civil_time(step(T{}, a.f_, n));
  }
  inline friend CONSTEXPR civil_time operator-(const civil_time& a, int n) {
    return civil_time(step(T{}, a.f_, -n));
  }
  inline friend CONSTEXPR int operator-(const civil_time& lhs,
                                        const civil_time& rhs) {
    return difference(T{}, lhs.f_, rhs.f_);
  }

 private:
  // All instantiations of this template are allowed to call the following
  // private constructor and access the private fields member.
  template <typename U>
  friend class civil_time;

  // The designated constructor that all others eventually call.
  explicit CONSTEXPR civil_time(fields f) : f_(align(T{}, f)) {}

  fields f_;
};

using civil_year = civil_time<year_tag>;
using civil_month = civil_time<month_tag>;
using civil_day = civil_time<day_tag>;
using civil_hour = civil_time<hour_tag>;
using civil_minute = civil_time<minute_tag>;
using civil_second = civil_time<second_tag>;

////////////////////////////////////////////////////////////////////////

// Relational operators that work with differently aligned objects.
// Always compares all six fields.
template <typename T1, typename T2>
CONSTEXPR bool operator<(const civil_time<T1>& lhs,
                         const civil_time<T2>& rhs) {
  return (lhs.year() < rhs.year() ||
          (lhs.year() == rhs.year() &&
           (lhs.month() < rhs.month() ||
            (lhs.month() == rhs.month() &&
             (lhs.day() < rhs.day() ||
              (lhs.day() == rhs.day() &&
               (lhs.hour() < rhs.hour() ||
                (lhs.hour() == rhs.hour() &&
                 (lhs.minute() < rhs.minute() ||
                  (lhs.minute() == rhs.minute() &&
                   (lhs.second() < rhs.second())))))))))));
}
template <typename T1, typename T2>
CONSTEXPR bool operator<=(const civil_time<T1>& lhs,
                          const civil_time<T2>& rhs) {
  return !(rhs < lhs);
}
template <typename T1, typename T2>
CONSTEXPR bool operator>=(const civil_time<T1>& lhs,
                          const civil_time<T2>& rhs) {
  return !(lhs < rhs);
}
template <typename T1, typename T2>
CONSTEXPR bool operator>(const civil_time<T1>& lhs,
                         const civil_time<T2>& rhs) {
  return rhs < lhs;
}
template <typename T1, typename T2>
CONSTEXPR bool operator==(const civil_time<T1>& lhs,
                          const civil_time<T2>& rhs) {
  return lhs.year() == rhs.year() && lhs.month() == rhs.month() &&
         lhs.day() == rhs.day() && lhs.hour() == rhs.hour() &&
         lhs.minute() == rhs.minute() && lhs.second() == rhs.second();
}
template <typename T1, typename T2>
CONSTEXPR bool operator!=(const civil_time<T1>& lhs,
                          const civil_time<T2>& rhs) {
  return !(lhs == rhs);
}

////////////////////////////////////////////////////////////////////////

// Output stream operators output a format matching YYYY-MM-DDThh:mm:ss,
// while omitting fields inferior to the type's alignment. For example,
// civil_day is formatted only as YYYY-MM-DD.
inline std::ostream& operator<<(std::ostream& os, civil_year y) {
  std::stringstream ss;
  ss << y.year();  // No padding.
  return os << ss.str();
}
inline std::ostream& operator<<(std::ostream& os, civil_month m) {
  std::stringstream ss;
  ss << civil_year(m) << '-';
  ss << std::setfill('0') << std::setw(2) << m.month();
  return os << ss.str();
}
inline std::ostream& operator<<(std::ostream& os, civil_day d) {
  std::stringstream ss;
  ss << civil_month(d) << '-';
  ss << std::setfill('0') << std::setw(2) << d.day();
  return os << ss.str();
}
inline std::ostream& operator<<(std::ostream& os, civil_hour h) {
  std::stringstream ss;
  ss << civil_day(h) << 'T';
  ss << std::setfill('0') << std::setw(2) << h.hour();
  return os << ss.str();
}
inline std::ostream& operator<<(std::ostream& os, civil_minute m) {
  std::stringstream ss;
  ss << civil_hour(m) << ':';
  ss << std::setfill('0') << std::setw(2) << m.minute();
  return os << ss.str();
}
inline std::ostream& operator<<(std::ostream& os, civil_second s) {
  std::stringstream ss;
  ss << civil_minute(s) << ':';
  ss << std::setfill('0') << std::setw(2) << s.second();
  return os << ss.str();
}

////////////////////////////////////////////////////////////////////////

enum class weekday {
  monday,
  tuesday,
  wednesday,
  thursday,
  friday,
  saturday,
  sunday,
};

inline std::ostream& operator<<(std::ostream& os, weekday wd) {
  switch (wd) {
    case weekday::monday:
      return os << "Monday";
    case weekday::tuesday:
      return os << "Tuesday";
    case weekday::wednesday:
      return os << "Wednesday";
    case weekday::thursday:
      return os << "Thursday";
    case weekday::friday:
      return os << "Friday";
    case weekday::saturday:
      return os << "Saturday";
    case weekday::sunday:
      return os << "Sunday";
  }
}

inline CONSTEXPR weekday get_weekday(const civil_day& cd) {
  CONSTEXPR const weekday k_weekday_by_thu_off[] = {
      weekday::thursday,  weekday::friday,  weekday::saturday,
      weekday::sunday,    weekday::monday,  weekday::tuesday,
      weekday::wednesday,
  };
  return k_weekday_by_thu_off[((cd - civil_day()) % 7 + 7) % 7];
}

////////////////////////////////////////////////////////////////////////

inline CONSTEXPR civil_day next_weekday(civil_day cd, weekday wd) {
  do { cd += 1; } while (get_weekday(cd) != wd);
  return cd;
}

inline CONSTEXPR civil_day prev_weekday(civil_day cd, weekday wd) {
  do { cd -= 1; } while (get_weekday(cd) != wd);
  return cd;
}

inline CONSTEXPR int get_yearday(const civil_day& cd) {
  return cd - civil_day(civil_year(cd)) + 1;
}

}  // namespace detail
}  // namespace cctz

#undef CONSTEXPR
