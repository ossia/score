#pragma once
#include <Process/ZoomHelper.hpp>

#include <ossia-qt/time_value.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <cmath>

#include <flicks.h>

#include <score_lib_process_export.h>

#include <chrono>
#include <verdigris>

class QTime;
struct SCORE_LIB_PROCESS_EXPORT TimeVal : ossia::time_value
{
  using ossia::time_value::time_value;

  static constexpr TimeVal fromMsecs(double msecs)
  {
    TimeVal time;
    time.impl = msecs * ossia::flicks_per_millisecond<double>;
    return time;
  }
  static constexpr TimeVal fromPixels(double pixels, double flicksPerPixel)
  {
    TimeVal time;
    time.impl = pixels * flicksPerPixel;
    return time;
  }

  constexpr TimeVal() noexcept : time_value{0} { }

  ~TimeVal() = default;
  constexpr TimeVal(const TimeVal&) = default;
  constexpr TimeVal(TimeVal&&) noexcept = default;
  constexpr TimeVal& operator=(const TimeVal&) = default;
  constexpr TimeVal& operator=(TimeVal&&) noexcept = default;

  constexpr TimeVal(ossia::time_value v) noexcept : time_value{v} { }
  explicit constexpr TimeVal(int64_t v) noexcept : time_value{v} { }

  static constexpr TimeVal zero() noexcept { return TimeVal{time_value{}}; }

  explicit TimeVal(const QTime& t) noexcept;

  template <
      typename Duration,
      std::enable_if_t<std::is_class<typename Duration::period>::value>* = nullptr>
  constexpr TimeVal(Duration&& dur) noexcept : time_value{util::flicks_cast(dur).count()}
  {
  }

  constexpr TimeVal operator-(TimeVal t) const noexcept
  {
    if (infinite() || t.infinite())
      return TimeVal{infinity};

    return TimeVal{impl - t.impl};
  }
  constexpr TimeVal operator+(TimeVal t) const noexcept
  {
    if (infinite() || t.infinite())
      return TimeVal{infinity};

    return TimeVal{impl + t.impl};
  }

  constexpr TimeVal& operator=(bool d) noexcept = delete;
  constexpr TimeVal& operator=(double d) noexcept = delete;
  constexpr TimeVal& operator=(float d) noexcept = delete;
  constexpr TimeVal& operator=(uint64_t d) noexcept = delete;

  constexpr TimeVal& operator=(int64_t d) noexcept
  {
    impl = d;
    return *this;
  }
  constexpr TimeVal& operator=(int32_t d) noexcept
  {
    impl = d;
    return *this;
  }

  constexpr TimeVal& operator-() noexcept
  {
    if (!infinite())
      impl = -impl;

    return *this;
  }

  constexpr time_value operator*(time_value d) const noexcept { return time_value{impl * d.impl}; }

  constexpr time_value operator*(double d) const noexcept
  {
    time_value res = *this;
    res.impl *= d;
    return res;
  }
  constexpr time_value operator*(int64_t d) const noexcept { return time_value{impl * d}; }

  operator bool() const noexcept = delete;

  constexpr double msec() const noexcept
  {
    if (!infinite())
      return impl / ossia::flicks_per_millisecond<double>;

    return 0;
  }

  constexpr double sec() const noexcept
  {
    if (!infinite())
      return double(impl) / ossia::flicks_per_second<double>;
    return 0;
  }

  constexpr double toPixels(ZoomRatio ratio) const noexcept
  {
    return (ratio > 0 && !infinite()) ? std::round(impl / ratio) : 0;
  }

  constexpr int64_t toSample(double sampleRate) const noexcept
  {
    return ossia::to_sample(*this, sampleRate);
  }

  QTime toQTime() const noexcept;
  QString toString() const noexcept;

  constexpr void setMSecs(double msecs) noexcept
  {
    impl = msecs * ossia::flicks_per_millisecond<double>;
  }

  constexpr bool operator==(TimeVal other) const noexcept { return impl == other.impl; }

  constexpr bool operator!=(TimeVal other) const noexcept { return impl != other.impl; }

  constexpr bool operator>(TimeVal other) const noexcept { return impl > other.impl; }

  constexpr bool operator>=(TimeVal other) const noexcept { return impl >= other.impl; }

  constexpr bool operator<(TimeVal other) const noexcept { return impl < other.impl; }

  constexpr bool operator<=(TimeVal other) const noexcept { return impl <= other.impl; }

  constexpr bool operator==(time_value other) const noexcept { return impl == other.impl; }

  constexpr bool operator!=(time_value other) const noexcept { return impl != other.impl; }

  constexpr bool operator>(time_value other) const noexcept { return impl > other.impl; }

  constexpr bool operator>=(time_value other) const noexcept { return impl >= other.impl; }

  constexpr bool operator<(time_value other) const noexcept { return impl < other.impl; }

  constexpr bool operator<=(time_value other) const noexcept { return impl <= other.impl; }
};

inline const TimeVal& max(const TimeVal& lhs, const TimeVal& rhs) noexcept
{
  if (lhs < rhs)
    return rhs;
  else
    return lhs;
}

#include <qhash.h>
namespace std
{
template <>
struct hash<TimeVal>
{
  std::size_t operator()(const TimeVal& t) const { return qHash(t.impl); }
};
}

Q_DECLARE_METATYPE(TimeVal)
W_REGISTER_ARGTYPE(TimeVal)
