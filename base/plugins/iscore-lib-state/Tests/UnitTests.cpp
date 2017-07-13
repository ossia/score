// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/dataspace/dataspace.hpp>
#include <QtTest/QtTest>
#include <State/Unit.hpp>
#include <core/application/MockApplication.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

class UnitTests : public QObject
{
  Q_OBJECT
  iscore::testing::MockApplication m;

private slots:

  void test_deserialize()
  {

    ossia::unit_t u = ossia::rgba_u{};
    QVERIFY(u == u);

    auto v = iscore::unmarshall<ossia::unit_t>(iscore::marshall<DataStream>(u));

    QVERIFY(u == v);
  }
};

QTEST_APPLESS_MAIN(UnitTests)
#include "UnitTests.moc"
