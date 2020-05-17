// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <State/Message.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <core/application/MockApplication.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QMetaType>
#include <QObject>

using namespace score;
class SerializationTest : public QObject
{
  Q_OBJECT
  score::testing::MockApplication m;

public:
private:
  void serializationTest()
  {
    using namespace score;
    QMetaType::registerComparators<State::Message>();
    qRegisterMetaTypeStreamOperators<State::Message>();
    qRegisterMetaTypeStreamOperators<State::MessageList>();
    qRegisterMetaTypeStreamOperators<ossia::value>();
    State::Message m;
    m.address = {"dada", {"bilou", "yadaa", "zoo"}};
    m.value = 5.5f;

    {
      auto json = marshall<JSONObject>(m);
      auto mess_json = unmarshall<State::Message>(json);
      SCORE_ASSERT(m == mess_json);

      auto barray = marshall<DataStream>(m);
      auto mess_array = unmarshall<State::Message>(barray);
      SCORE_ASSERT(m == mess_array);
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
      ossia::value v = ossia::impulse{};
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
      ossia::value v(ossia::vec2f{1, 2});
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(ossia::vec3f{1, 2, 45});
      QCOMPARE(unmarshall<ossia::value>(marshall<JSONObject>(v)), v);
      QCOMPARE(unmarshall<ossia::value>(marshall<DataStream>(v)), v);
    }
    {
      ossia::value v(ossia::vec4f{1, 2, 45, 345});
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
      ossia::domain d;
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::domain d = ossia::make_domain(0, 1);
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::domain d = ossia::make_domain(0., 1.);
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::domain d = ossia::make_domain(false, true);
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::domain d = ossia::make_domain('a', 'z');
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::domain d = ossia::make_domain(ossia::vec2f{0, 0}, ossia::vec2f{1, 1});
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::domain d = ossia::make_domain(ossia::vec3f{0, 0, 0}, ossia::vec3f{1, 1, 1});
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::domain d = ossia::make_domain(ossia::vec4f{0, 0, 0, 0}, ossia::vec4f{1, 1, 1, 1});
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::domain d = ossia::make_domain(
          std::vector<ossia::value>{0, 'x'}, std::vector<ossia::value>{1, 'y'});
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }

    {
      ossia::domain d = ossia::domain_base<std::string>();
      QCOMPARE(unmarshall<ossia::domain>(marshall<JSONObject>(d)), d);
      QCOMPARE(unmarshall<ossia::domain>(marshall<DataStream>(d)), d);
    }
  }
};

QTEST_APPLESS_MAIN(SerializationTest)
#include "SerializationTest.moc"

#include <State/Address.hpp>
#include <State/Value.hpp>

class DataStream;
class JSONObject;
