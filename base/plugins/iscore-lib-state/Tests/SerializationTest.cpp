#include <ossia/network/domain/domain.hpp>
#include <QMetaType>
#include <QObject>
#include <QtTest/QtTest>
#include <State/Message.hpp>
#include <core/application/MockApplication.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

using namespace iscore;
class SerializationTest : public QObject
{
  Q_OBJECT
  iscore::testing::MockApplication m;

public:
private slots:

  void serializationTest()
  {
    using namespace iscore;
    QMetaType::registerComparators<State::Message>();
    qRegisterMetaTypeStreamOperators<State::Message>();
    qRegisterMetaTypeStreamOperators<State::MessageList>();
    qRegisterMetaTypeStreamOperators<State::Value>();
    State::Message m;
    m.address = {"dada", {"bilou", "yadaa", "zoo"}};
    m.value.val = 5.5f;

    {
      auto json = marshall<JSONObject>(m);
      auto mess_json = unmarshall<State::Message>(json);
      ISCORE_ASSERT(m == mess_json);

      auto barray = marshall<DataStream>(m);
      auto mess_array = unmarshall<State::Message>(barray);
      ISCORE_ASSERT(m == mess_array);
    }
  }
  void ossia_value_serialization_test()
  {
    using namespace std::literals;
    {
      ossia::value v;
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v = ossia::Impulse{};
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(1234);
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(1234.f);
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v('c');
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v("duh"s);
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(ossia::Vec2f{1, 2});
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(ossia::Vec3f{1, 2, 45});
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(ossia::Vec4f{1, 2, 45, 345});
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(std::vector<ossia::value>{});
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(std::vector<ossia::value>{1, 2, 3});
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(std::vector<ossia::value>{
          1, "boo"s, 3, std::vector<ossia::value>{"Banana"s, "Carrot"s, 'c'}});
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
    }
  }

  void ossia_domain_serialization_test()
  {
    {
      ossia::net::domain d;
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::net::domain d = ossia::net::make_domain(0, 1);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::net::domain d = ossia::net::make_domain(0., 1.);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::net::domain d = ossia::net::make_domain(false, true);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::net::domain d = ossia::net::make_domain('a', 'z');
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::net::domain d
          = ossia::net::make_domain(ossia::Vec2f{0, 0}, ossia::Vec2f{1, 1});
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::net::domain d = ossia::net::make_domain(
          ossia::Vec3f{0, 0, 0}, ossia::Vec3f{1, 1, 1});
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::net::domain d = ossia::net::make_domain(
          ossia::Vec4f{0, 0, 0, 0}, ossia::Vec4f{1, 1, 1, 1});
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::net::domain d = ossia::net::make_domain(
          std::vector<ossia::value>{0, 'x'},
          std::vector<ossia::value>{1, 'y'});
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::net::domain d = ossia::net::domain_base<std::string>();
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::net::domain>(marshall<DataStream>(d)), d);
    }
  }
};

QTEST_APPLESS_MAIN(SerializationTest)
#include "SerializationTest.moc"
#include <State/Address.hpp>
#include <State/Value.hpp>

class DataStream;
class JSONObject;
