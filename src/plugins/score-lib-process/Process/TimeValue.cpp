#include <Process/TimeValue.hpp>
#include <QTime>

TimeVal::TimeVal(const QTime& t) noexcept
  : time_value{int64_t(
                 ossia::flicks_per_millisecond<
                 double> * (t.msec() + 1000. * t.second() + 60000. * t.minute() + 3600000. * t.hour()))}
{
}

QTime TimeVal::toQTime() const noexcept
{
  if (infinite())
    return QTime(23, 59, 59, 999);
  else
    return QTime(0, 0, 0, 0).addMSecs(msec());
}

QString TimeVal::toString() const noexcept
{
  auto qT = this->toQTime();
  return QString("%1%2%3 s %4 ms")
      .arg(
          qT.hour() != 0 ? QString::number(qT.hour()) % QStringLiteral(" h ") : QString(),
          qT.minute() != 0 ? QString::number(qT.minute()) % QStringLiteral(" min ") : QString(),
          QString::number(qT.second()),
          QString::number(qT.msec()));
}

