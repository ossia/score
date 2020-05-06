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
struct TimeVal : ossia::time_value
{
  using ossia::time_value::time_value;

  template<typename T>
  static constexpr TimeVal fromMsecs(T msecs)
  {
    TimeVal time;
    time.impl = msecs * ossia::flicks_per_millisecond<T>;
    return time;
  }


  constexpr TimeVal() noexcept
    : time_value{0}
  {

  }

  constexpr ~TimeVal() = default;
  constexpr TimeVal(const TimeVal&) = default;
  constexpr TimeVal(TimeVal&&) noexcept = default;
  constexpr TimeVal& operator=(const TimeVal&) = default;
  constexpr TimeVal& operator=(TimeVal&&) noexcept = default;

  constexpr TimeVal(ossia::time_value v) noexcept: time_value{v} { }
  explicit constexpr TimeVal(int64_t v) noexcept: time_value{v} { }

  static constexpr TimeVal zero() noexcept
  {
    return TimeVal{time_value{}};
  }

  TimeVal(QTime t) noexcept
      : time_value{int64_t(ossia::flicks_per_millisecond<double> *
            (t.msec() + 1000. * t.second() + 60000. * t.minute()
              + 3600000. * t.hour()))}
  {
  }


  template <
      typename Duration,
      std::enable_if_t<
          std::is_class<typename Duration::period>::value>* = nullptr>
  constexpr TimeVal(Duration&& dur) noexcept
      : time_value{util::flicks_cast(dur).count()}
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

  constexpr bool operator==(TimeVal other) const noexcept
  {
    return impl == other.impl;
  }

  constexpr bool operator!=(TimeVal other) const noexcept
  {
    return impl != other.impl;
  }

  constexpr bool operator>(TimeVal other) const noexcept
  {
    return impl > other.impl;
  }

  constexpr bool operator>=(TimeVal other) const noexcept
  {
    return impl >= other.impl;
  }

  constexpr bool operator<(TimeVal other) const noexcept
  {
    return impl < other.impl;
  }

  constexpr bool operator<=(TimeVal other) const noexcept
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
