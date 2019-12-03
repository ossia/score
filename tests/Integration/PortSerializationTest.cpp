// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <State/Message.hpp>
#include <State/Expression.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/application/MockApplication.hpp>

#include <ossia/editor/state/message.hpp>

#include <QObject>
#include <QtTest>

using namespace score;
class PortSerializationTest : public QObject
{
  Q_OBJECT
  score::testing::MockApplication m;
  Process::PortFactoryList pl;

public:
  PortSerializationTest()
  {
    pl.insert(std::make_unique<Dataflow::InletFactory>());
    pl.insert(std::make_unique<Dataflow::ControlInletFactory>());
    pl.insert(std::make_unique<Dataflow::OutletFactory>());
    pl.insert(std::make_unique<Dataflow::AudioOutletFactory>());
    pl.insert(std::make_unique<Dataflow::ControlOutletFactory>());
  }

private Q_SLOTS:
  void test_controlinlet_json_upcast()
  {
    Process::ControlInlet port{Id<Process::Port>{1234}, nullptr};
    port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));
    port.setValue(2.5f);
    {
      auto json = marshall<JSONObject>((Process::Inlet&)port);
      auto new_port = deserialize_interface(pl, JSONObject::Deserializer{json},
nullptr); auto ptr = dynamic_cast<Process::ControlInlet*>(new_port);
      QVERIFY(ptr);
      QVERIFY(ptr->id() == port.id());
      QVERIFY(ptr->address() == port.address());
      QVERIFY(ptr->value() == port.value());
    }
  }
  void test_controlinlet_json_no_upcast()
  {
      Process::ControlInlet port{Id<Process::Port>{1234}, nullptr};
      port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));
      port.setValue(2.5f);
      auto p = marshall<JSONObject>(port);
      {
          auto json = marshall<JSONObject>(port);
          auto new_port = deserialize_interface(pl, JSONObject::Deserializer{json}, nullptr);
          auto ptr = dynamic_cast<Process::ControlInlet*>(new_port);
          QVERIFY(ptr);
          QVERIFY(ptr->id() == port.id());
          QVERIFY(ptr->address() == port.address());
          QVERIFY(ptr->value() == port.value());
      }
  }

  void test_controloutlet_json_upcast()
  {
    Process::ControlOutlet port{Id<Process::Port>{1234}, nullptr};
    port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));
    port.setValue(2.5f);

    {
      auto json = marshall<JSONObject>((Process::Outlet&)port);
      auto new_port = deserialize_interface(pl, JSONObject::Deserializer{json}, nullptr);
      auto ptr = dynamic_cast<Process::ControlOutlet*>(new_port);
      QVERIFY(ptr);
      QVERIFY(ptr->id() == port.id());
      QVERIFY(ptr->address() == port.address());
      QVERIFY(ptr->value() == port.value());
    }
  }

  void test_controloutlet_json_no_upcast()
  {
      Process::ControlOutlet port{Id<Process::Port>{1234}, nullptr};
      port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));
      port.setValue(2.5f);

      {
          auto json = marshall<JSONObject>(port);
          auto new_port = deserialize_interface(pl, JSONObject::Deserializer{json},
                                                nullptr);
          auto ptr = dynamic_cast<Process::ControlOutlet*>(new_port);
          QVERIFY(ptr);
          QVERIFY(ptr->id() == port.id());
          QVERIFY(ptr->address() == port.address());
          QVERIFY(ptr->value() == port.value());
      }
  }

  void test_controlinlet_datastream_upcast()
  {
    Process::ControlInlet port{Id<Process::Port>{1234}, nullptr};
    port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));
    port.setValue(2.5f);

    {
      auto json = marshall<DataStream>((Process::Inlet&)port);
      auto new_port = deserialize_interface(pl, DataStream::Deserializer{json}, nullptr);
      auto ptr = dynamic_cast<Process::ControlInlet*>(new_port);
      QVERIFY(ptr);
      QVERIFY(ptr->id() == port.id());
      QVERIFY(ptr->address() == port.address());
      QVERIFY(ptr->value() == port.value());
    }
  }

  void test_controlinlet_datastream_no_upcast()
  {
      Process::ControlInlet port{Id<Process::Port>{1234}, nullptr};
      port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));
      port.setValue(2.5f);

      {
          auto json = marshall<DataStream>(port);
          auto new_port = deserialize_interface(pl, DataStream::Deserializer{json},
                                                nullptr);
          auto ptr = dynamic_cast<Process::ControlInlet*>(new_port);
          QVERIFY(ptr);
          QVERIFY(ptr->id() == port.id());
          QVERIFY(ptr->address() == port.address());
          QVERIFY(ptr->value() == port.value());
      }
  }

  void test_controloutlet_datastream_upcast()
  {
    Process::ControlOutlet port{Id<Process::Port>{1234}, nullptr};
    port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));
    port.setValue(2.5f);

    {
      auto json = marshall<DataStream>((Process::Outlet&)port);
      auto new_port = deserialize_interface(pl, DataStream::Deserializer{json}, nullptr);
      auto ptr = dynamic_cast<Process::ControlOutlet*>(new_port);
      QVERIFY(ptr);
      QVERIFY(ptr->id() == port.id());
      QVERIFY(ptr->address() == port.address());
      QVERIFY(ptr->value() == port.value());
    }
  }


  void test_inlet_json_upcast()
  {
    Process::Inlet port{Id<Process::Port>{1234}, nullptr};
    port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));

    {
      auto json = marshall<JSONObject>((Process::Inlet&)port);
      auto new_port = deserialize_interface(pl, JSONObject::Deserializer{json}, nullptr);
      auto ptr = dynamic_cast<Process::Inlet*>(new_port); QVERIFY(ptr);
      QVERIFY(ptr->id() == port.id());
      QVERIFY(ptr->address() == port.address());
    }
  }

  void test_outlet_json_upcast()
  {
    Process::Outlet port{Id<Process::Port>{1234}, nullptr};
    port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));

    {
      auto json = marshall<JSONObject>((Process::Outlet&)port);
      auto new_port = deserialize_interface(pl, JSONObject::Deserializer{json},nullptr);
      auto ptr = dynamic_cast<Process::Outlet*>(new_port); QVERIFY(ptr);
      QVERIFY(ptr->id() == port.id());
      QVERIFY(ptr->address() == port.address());
    }
  }

  void test_inlet_datastream_upcast()
  {
    Process::Inlet port{Id<Process::Port>{1234}, nullptr};
    port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));

    {
      auto json = marshall<DataStream>((Process::Inlet&)port);
      auto new_port = deserialize_interface(pl, DataStream::Deserializer{json}, nullptr);
      auto ptr = dynamic_cast<Process::Inlet*>(new_port); QVERIFY(ptr);
      QVERIFY(ptr->id() == port.id());
      QVERIFY(ptr->address() == port.address());
    }
  }

  void test_outlet_datastream_upcast()
  {
    Process::Outlet port{Id<Process::Port>{1234}, nullptr};
    port.setAddress(*State::parseAddressAccessor("foo:/bar@[1]"));

    {
      auto json = marshall<DataStream>((Process::Outlet&)port);
      auto new_port = deserialize_interface(pl, DataStream::Deserializer{json}, nullptr);
      auto ptr = dynamic_cast<Process::Outlet*>(new_port); QVERIFY(ptr);
      QVERIFY(ptr->id() == port.id());
      QVERIFY(ptr->address() == port.address());
    }
  }
};

QTEST_APPLESS_MAIN(PortSerializationTest)
#include "PortSerializationTest.moc"
