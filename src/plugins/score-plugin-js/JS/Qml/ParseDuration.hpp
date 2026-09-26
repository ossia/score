#pragma once
#include <Process/TimeValue.hpp>

#include <QString>
#include <QStringList>

#include <cmath>
#include <optional>

namespace JS
{
//! Duration arguments of the scripting API:
//! - a number: flicks, as setIntervalDuration and the numeric createBox take;
//! - a number with a unit: "500ms", "2s", "1.5 min", "1h";
//! - a clock time: "SS", "M:SS" or "H:MM:SS", each with an optional fraction
//!   of a second ("1:02:03.250"); hours are not limited to 23.
//! Negative or unparsable input gives nullopt.
inline std::optional<TimeVal> parseDuration(const QString& text)
{
  const QString s = text.trimmed();
  if(s.isEmpty())
    return std::nullopt;

  // llround is undefined past the int64 range
  const auto fromFlicks = [](double flicks) -> std::optional<TimeVal> {
    if(!(flicks >= 0. && flicks < 0x1p63))
      return std::nullopt;
    return TimeVal{int64_t(std::llround(flicks))};
  };
  const auto fromSeconds = [&](double sec) {
    return fromFlicks(sec * ossia::flicks_per_second<double>);
  };

  bool ok = false;
  if(const double flicks = s.toDouble(&ok); ok)
    return fromFlicks(flicks);

  static constexpr struct
  {
    const char* suffix;
    double seconds;
  } units[] = {{"ms", 1e-3}, {"min", 60.}, {"s", 1.}, {"h", 3600.}};
  for(const auto& u : units)
  {
    if(s.endsWith(QLatin1String(u.suffix), Qt::CaseInsensitive))
    {
      const double v = s.chopped(qsizetype(strlen(u.suffix))).trimmed().toDouble(&ok);
      return ok ? fromSeconds(v * u.seconds) : std::nullopt;
    }
  }

  const QStringList parts = s.split(QLatin1Char(':'));
  if(parts.size() < 2 || parts.size() > 3)
    return std::nullopt;
  double sec = 0.;
  for(qsizetype i = 0; i < parts.size(); ++i)
  {
    const bool last = i == parts.size() - 1;
    const double v = last ? parts[i].toDouble(&ok) : double(parts[i].toUInt(&ok));
    if(!ok || v < 0. || (i > 0 && v >= 60.))
      return std::nullopt;
    sec = sec * 60. + v;
  }
  return fromSeconds(sec);
}
}
