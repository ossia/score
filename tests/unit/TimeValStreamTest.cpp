#include <Process/TimeValue.hpp>

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("a TimeVal streams through QDataStream without an application", "[serialization]")
{
  const TimeVal t{int64_t(123456789012345)};
  QByteArray bytes;
  {
    QDataStream out{&bytes, QIODevice::WriteOnly};
    out << t;
  }
  // The format DataStreamReader writes: the flicks as a big-endian int64.
  QByteArray expected;
  {
    QDataStream ref{&expected, QIODevice::WriteOnly};
    ref << qint64(t.impl);
  }
  CHECK(bytes == expected);

  TimeVal back;
  QDataStream in{bytes};
  in >> back;
  CHECK(back == t);
}
