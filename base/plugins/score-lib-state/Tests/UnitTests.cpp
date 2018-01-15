// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/network/dataspace/dataspace.hpp>
#include <QtTest/QtTest>
#include <State/Unit.hpp>
#include <core/application/MockApplication.hpp>
#include <score/serialization/VisitorCommon.hpp>

class UnitTests : public QObject
{
  Q_OBJECT
  score::testing::MockApplication m;

private Q_SLOTS:

  void test_deserialize()
  {

    ossia::unit_t u = ossia::rgba_u{};
    QVERIFY(u == u);

    auto v = score::unmarshall<ossia::unit_t>(score::marshall<DataStream>(u));

    QVERIFY(u == v);
  }
};

QTEST_APPLESS_MAIN(UnitTests)
#include "UnitTests.moc"
