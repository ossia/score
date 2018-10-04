#pragma once
#include <Process/ZoomHelper.hpp>

#include <score/serialization/IsTemplate.hpp>
#include <score/tools/std/Optional.hpp>

#include <QMetaType>
#include <QStringBuilder>
#include <QTime>

#include <wobjectdefs.h>

#include <chrono>
#if defined(_MSC_VER)
#define OPTIONAL_CONSTEXPR constexpr
#else
#define OPTIONAL_CONSTEXPR
#endif
struct TimeValue_T
{
  using T = double;
  static OPTIONAL_CONSTEXPR TimeValue_T zero()
  {
    return TimeValue_T{optional<T>{0}};
  }
  static OPTIONAL_CONSTEXPR TimeValue_T infinite()
  {
    return TimeValue_T{optional<T>{}};
  }

  OPTIONAL_CONSTEXPR static TimeValue_T fromMsecs(T msecs)
  {
    TimeValue_T time;
    time.m_impl = msecs;
    return time;
  }

  OPTIONAL_CONSTEXPR TimeValue_T() = default;
  //~TimeValue_T() = default;
  OPTIONAL_CONSTEXPR TimeValue_T(const TimeValue_T&) = default;
  OPTIONAL_CONSTEXPR TimeValue_T(TimeValue_T&&) = default;
  OPTIONAL_CONSTEXPR TimeValue_T& operator=(const TimeValue_T&) = default;
  OPTIONAL_CONSTEXPR TimeValue_T& operator=(TimeValue_T&&) = default;
  OPTIONAL_CONSTEXPR TimeValue_T(optional<T> t) : m_impl{std::move(t)}
  {
  }

  TimeValue_T(QTime t)
      : m_impl{
            T(t.msec() + 1000 * t.second() + 60000 * t.minute()
              + 3600000 * t.hour())}
  {
  }

  // These two overloads are here to please coverity...
  OPTIONAL_CONSTEXPR TimeValue_T(std::chrono::seconds&& dur)
      : m_impl{T(std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                     .count())}
  {
  }
  OPTIONAL_CONSTEXPR TimeValue_T(std::chrono::milliseconds&& dur)
      : m_impl{T(dur.count())}
  {
  }

  template <
      typename Duration, std::enable_if_t<std::is_class<
                             typename Duration::period>::value>* = nullptr>
  OPTIONAL_CONSTEXPR TimeValue_T(Duration&& dur)
      : m_impl{T(std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                     .count())}
  {
  }

  bool isInfinite() const
  {
    return !bool(m_impl);
  }

  bool isZero() const
  {
    return !isInfinite() && (msec() == 0);
  }

  T msec() const
  {
    if (!isInfinite())
      return *m_impl;

    return 0;
  }

  T sec() const
  {
    return double(*m_impl) / 1000;
  }

  double toPixels(ZoomRatio ratio) const
  {
    return (ratio > 0 && !isInfinite()) ? *m_impl / ratio : 0;
  }

  QTime toQTime() const
  {
    if (isInfinite())
      return QTime(23, 59, 59, 999);
    else
      return QTime(0, 0, 0, 0).addMSecs(static_cast<int>(*m_impl));
  }

  QString toString() const
  {
    auto qT = this->toQTime();
    return QString("%1%2%3 s %4 ms")
        .arg(
            qT.hour() != 0 ? QString::number(qT.hour()) % QStringLiteral(" h ")
                           : QString(),
            qT.minute() != 0
                ? QString::number(qT.minute()) % QStringLiteral(" min ")
                : QString(),
            QString::number(qT.second()), QString::number(qT.msec()));
  }

  void addMSecs(T msecs)
  {
    if (m_impl)
    {
      *m_impl += msecs;
    }
  }

  void setMSecs(T msecs)
  {
    m_impl = msecs;
  }

  bool operator==(const TimeValue_T& other) const
  {
    return other.m_impl == m_impl;
  }

  bool operator!=(const TimeValue_T& other) const
  {
    return other.m_impl != m_impl;
  }

  bool operator>(const TimeValue_T& other) const
  {
    if (isInfinite() && other.isInfinite())
    {
      return false;
    }
    else if (isInfinite() && !other.isInfinite())
    {
      return true;
    }
    else if (!isInfinite() && other.isInfinite())
    {
      return false;
    }
    else
    {
      return msec() > other.msec();
    }
  }

  bool operator>=(const TimeValue_T& other) const
  {
    return *this > other || *this == other;
  }

  bool operator<(const TimeValue_T& other) const
  {
    if (isInfinite() && other.isInfinite())
    {
      return false;
    }
    else if (!isInfinite() && other.isInfinite())
    {
      return true;
    }
    else if (isInfinite() && !other.isInfinite())
    {
      return false;
    }
    else
    {
      return msec() < other.msec();
    }
  }

  bool operator<=(const TimeValue_T& other) const
  {
    return *this < other || *this == other;
  }

  TimeValue_T operator+(const TimeValue_T& other) const
  {
    TimeValue_T res = TimeValue_T::infinite();

    if (isInfinite() || other.isInfinite())
    {
      return res;
    }

    res.m_impl = *m_impl + *other.m_impl;
    return res;
  }

  TimeValue_T operator*(double other) const
  {
    TimeValue_T res = TimeValue_T::infinite();

    if (isInfinite())
    {
      return res;
    }

    res.m_impl = *m_impl * other;
    return res;
  }

  double operator/(const TimeValue_T& other) const
  {
    return double(*m_impl) / double(*other.m_impl);
  }

  TimeValue_T operator-(const TimeValue_T& other) const
  {
    TimeValue_T res = TimeValue_T::infinite();

    if (isInfinite() || other.isInfinite())
    {
      return res;
    }

    res.m_impl = *m_impl - *other.m_impl;
    return res;
  }

  TimeValue_T operator-() const
  {
    TimeValue_T res = TimeValue_T::zero();
    OPTIONAL_CONSTEXPR TimeValue_T zero = TimeValue_T::zero();

    res.m_impl = *zero.m_impl - *m_impl;

    return res;
  }

  TimeValue_T operator+=(const TimeValue_T& other)
  {
    *this = *this + other;
    return *this;
  }

  // We could use std::isinf instead but this would break
  // -Ofast...
  optional<T> m_impl{T(0.)};
};

using TimeVal = TimeValue_T;

inline const TimeVal& max(const TimeVal& lhs, const TimeVal& rhs)
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

Q_DECLARE_METATYPE(TimeVal)
W_REGISTER_ARGTYPE(TimeVal)
