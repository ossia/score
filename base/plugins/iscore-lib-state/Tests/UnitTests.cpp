#include <QtTest/QtTest>
#include <State/Unit.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <core/application/ApplicationInterface.hpp>

struct MockApplication final :
    public iscore::ApplicationInterface
{
public:

  MockApplication()
  {
    m_instance = this;
  }

  const iscore::ApplicationContext& context() const override
  {
    throw;
  }
  const iscore::ApplicationComponents& components() const override
  {
    static iscore::ApplicationComponentsData d0;
    static iscore::ApplicationComponents d1{d0};
    return d1;
  }
};

class UnitTests: public QObject
{
  Q_OBJECT
  MockApplication m;

private slots:


  void test_deserialize()
  {

    ossia::unit_t u = ossia::rgba_u{};
    QVERIFY(u == u);

    auto v = unmarshall<ossia::unit_t>(marshall<DataStream>(u));

    QVERIFY(u == v);

  }
};

QTEST_APPLESS_MAIN(UnitTests)
#include "UnitTests.moc"
