#pragma once
#include <Process/ZoomHelper.hpp>
#include <ossia/detail/flicks.hpp>
#include <flicks.h>
#include <ossia/editor/scenario/time_value.hpp>
#include <score/serialization/IsTemplate.hpp>
#include <score/tools/std/Optional.hpp>

#include <QTime>
#include <QStringBuilder>

#include <verdigris>

#include <chrono>
#if defined(_MSC_VER)
#define OPTIONAL_CONSTEXPR constexpr
#else
#define OPTIONAL_CONSTEXPR
#endif
struct TimeValue_T : ossia::time_value
{
  using ossia::time_value::time_value;

  template<typename T>
  static constexpr TimeValue_T fromMsecs(T msecs)
  {
    TimeValue_T time;
    time.impl = msecs * ossia::flicks_per_millisecond<T>;
    return time;
  }


  constexpr TimeValue_T() noexcept
    : time_value{0}
  {

  }

  constexpr ~TimeValue_T() = default;
  constexpr TimeValue_T(const TimeValue_T&) = default;
  constexpr TimeValue_T(TimeValue_T&&) noexcept = default;
  constexpr TimeValue_T& operator=(const TimeValue_T&) = default;
  constexpr TimeValue_T& operator=(TimeValue_T&&) noexcept = default;

  constexpr TimeValue_T(ossia::time_value v) noexcept: time_value{v} { }
  explicit constexpr TimeValue_T(int64_t v) noexcept: time_value{v} { }

  static constexpr TimeValue_T zero() noexcept
  {
    return TimeValue_T{time_value{}};
  }

  TimeValue_T(QTime t) noexcept
      : time_value{int64_t(ossia::flicks_per_millisecond<double> *
            (t.msec() + 1000. * t.second() + 60000. * t.minute()
              + 3600000. * t.hour()))}
  {
  }


  template <
      typename Duration,
      std::enable_if_t<
          std::is_class<typename Duration::period>::value>* = nullptr>
  OPTIONAL_CONSTEXPR TimeValue_T(Duration&& dur) noexcept
      : time_value{util::flicks_cast(dur).count()}
  {
  }

  constexpr TimeValue_T operator-(TimeValue_T t) const noexcept
  {
    if (infinite() || t.infinite())
      return TimeValue_T{infinity};

    return TimeValue_T{impl - t.impl};
  }
  constexpr TimeValue_T operator+(TimeValue_T t) const noexcept
  {
    if (infinite() || t.infinite())
      return TimeValue_T{infinity};

    return TimeValue_T{impl + t.impl};
  }


  constexpr TimeValue_T& operator=(bool d) noexcept = delete;
  constexpr TimeValue_T& operator=(double d) noexcept = delete;
  constexpr TimeValue_T& operator=(float d) noexcept = delete;
  constexpr TimeValue_T& operator=(uint64_t d) noexcept = delete;

  constexpr TimeValue_T& operator=(int64_t d) noexcept
  {
    impl = d;
    return *this;
  }
  constexpr TimeValue_T& operator=(int32_t d) noexcept
  {
    impl = d;
    return *this;
  }

  constexpr TimeValue_T& operator-() noexcept
  {
    if (!infinite())
      impl = -impl;

    return *this;
  }


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
    return (ratio > 0 && !infinite())
        ? impl / (ratio * ossia::flicks_per_millisecond<double>)
        : 0;
  }

  QTime toQTime() const noexcept
  {
    if (infinite())
      return QTime(23, 59, 59, 999);
    else
      return QTime(0, 0, 0, 0).addMSecs(msec());
  }

  QString toString() const noexcept
  {
    auto qT = this->toQTime();
    return QString("%1%2%3 s %4 ms")
        .arg(
            qT.hour() != 0 ? QString::number(qT.hour()) % QStringLiteral(" h ")
                           : QString(),
            qT.minute() != 0
                ? QString::number(qT.minute()) % QStringLiteral(" min ")
                : QString(),
            QString::number(qT.second()),
            QString::number(qT.msec()));
  }

  constexpr void setMSecs(double msecs) noexcept { impl = msecs * ossia::flicks_per_millisecond<double>; }

  constexpr bool operator==(TimeValue_T other) const noexcept
  {
    return impl == other.impl;
  }

  constexpr bool operator!=(TimeValue_T other) const noexcept
  {
    return impl != other.impl;
  }

  constexpr bool operator>(TimeValue_T other) const noexcept
  {
    return impl > other.impl;
  }

  constexpr bool operator>=(TimeValue_T other) const noexcept
  {
    return impl >= other.impl;
  }

  constexpr bool operator<(TimeValue_T other) const noexcept
  {
    return impl < other.impl;
  }

  constexpr bool operator<=(TimeValue_T other) const noexcept
  {
    return impl <= other.impl;
  }

  constexpr bool operator==(time_value other) const noexcept
  {
    return impl == other.impl;
  }

  constexpr bool operator!=(time_value other) const noexcept
  {
    return impl != other.impl;
  }

  constexpr bool operator>(time_value other) const noexcept
  {
    return impl > other.impl;
  }

  constexpr bool operator>=(time_value other) const noexcept
  {
    return impl >= other.impl;
  }

  constexpr bool operator<(time_value other) const noexcept
  {
    return impl < other.impl;
  }

  constexpr bool operator<=(time_value other) const noexcept
  {
    return impl <= other.impl;
  }

};

using TimeVal = TimeValue_T;

inline const TimeVal& max(const TimeVal& lhs, const TimeVal& rhs) noexcept
{
  if (lhs < rhs)
    return rhs;
  else
    return lhs;
}

template <>
struct is_custom_serialized<TimeVal> : std::true_type
{
};

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
