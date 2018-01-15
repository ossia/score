// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Device/Node/DeviceNode.hpp>
#include <QObject>
#include <QString>
#include <QtTest/QtTest>
#include <algorithm>
#include <core/application/MockApplication.hpp>
#include <score/serialization/AnySerialization.hpp>
#include <ossia/network/base/node_attributes.hpp>
class NodeTest : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void NodeTest_1()
  {
    Device::Node root;

    SCORE_ASSERT(root.is<InvisibleRootNode>());

    {
      Device::Node child(Device::AddressSettings{}, nullptr);
      root.push_back(child);

      SCORE_ASSERT(root.childCount() == 1);
      SCORE_ASSERT(root.children().back().parent() == &root);
    }

    {
      Device::Node child(Device::AddressSettings{}, &root);
      root.push_back(child);

      SCORE_ASSERT(root.childCount() == 2);
      SCORE_ASSERT(root.children().back().parent() == &root);
    }

    {
      Device::Node child(Device::DeviceSettings{}, &root);
      root.push_back(child);

      SCORE_ASSERT(root.childCount() == 3);
      SCORE_ASSERT(root.children().back().parent() == &root);
    }

    {
      Device::Node root_copy(root);

      SCORE_ASSERT(root.childCount() == 3);
      SCORE_ASSERT(root_copy.childCount() == 3);

      for (const auto& child : root_copy)
      {
        SCORE_ASSERT(child.parent() == &root_copy);
      }

      for (int i = 0; i < root.childCount(); i++)
      {
        SCORE_ASSERT(&root.children().at(i) != &root_copy.children().at(i));
        SCORE_ASSERT(
            root.children().at(i).which()
            == root_copy.children().at(i).which());
      }
    }

    {
      Device::Node root_copy;
      root_copy = root;
      root_copy = root;
      root_copy = root;

      SCORE_ASSERT(root.childCount() == 3);
      SCORE_ASSERT(root_copy.childCount() == 3);

      for (const auto& child : root_copy)
      {
        SCORE_ASSERT(child.parent() == &root_copy);
      }

      for (int i = 0; i < root.childCount(); i++)
      {
        SCORE_ASSERT(&root.children().at(i) != &root_copy.children().at(i));
        SCORE_ASSERT(
            root.children().at(i).which()
            == root_copy.children().at(i).which());
      }
    }

    {
      Device::Node root_copy(std::move(root));

      SCORE_ASSERT(root.childCount() == 0);
      SCORE_ASSERT(root_copy.childCount() == 3);

      for (const auto& child : root_copy)
      {
        SCORE_ASSERT(child.parent() == &root_copy);
      }

      SCORE_ASSERT(root_copy.children().at(0).is<Device::AddressSettings>());
      SCORE_ASSERT(root_copy.children().at(1).is<Device::AddressSettings>());
      SCORE_ASSERT(root_copy.children().at(2).is<Device::DeviceSettings>());

      root = std::move(root_copy);

      SCORE_ASSERT(root_copy.childCount() == 0);
      SCORE_ASSERT(root.childCount() == 3);

      for (const auto& child : root)
      {
        SCORE_ASSERT(child.parent() == &root);
      }

      SCORE_ASSERT(root.children().at(0).is<Device::AddressSettings>());
      SCORE_ASSERT(root.children().at(1).is<Device::AddressSettings>());
      SCORE_ASSERT(root.children().at(2).is<Device::DeviceSettings>());
    }
  }

  void NodeTest_2()
  {
    Device::Node root;

    Device::DeviceSettings dev_base{
        UuidKey<Device::ProtocolFactory>{"85783b8d-454d-4326-a070-9666d2534eff"}, "ADevice", {}};
    auto& dev = root.emplace_back(std::move(dev_base), nullptr);

    SCORE_ASSERT(root.childCount() == 1);
    SCORE_ASSERT(dev.parent() == &root);
    SCORE_ASSERT(&dev == &root.children()[0]);
    SCORE_ASSERT(&dev == &root.childAt(0));

    auto& dev_2 = root.emplace_back(
        Device::DeviceSettings{
            UuidKey<Device::ProtocolFactory>{"85783b8d-454d-4326-a070-9666d2534eff"}, "OtherDevice", {}},
        nullptr);
    SCORE_ASSERT(root.childCount() == 2);
    SCORE_ASSERT(dev_2.parent() == &root);

    Device::Node child(Device::AddressSettings{}, &dev_2);
    dev_2.push_back(child);

    SCORE_ASSERT(root.childCount() == 2);
    SCORE_ASSERT(dev.childCount() == 0);
    SCORE_ASSERT(dev_2.childCount() == 1);

    {
      Device::Node root_copy(root);

      SCORE_ASSERT(root.childCount() == 2);
      SCORE_ASSERT(root_copy.childCount() == 2);

      for (const auto& child : root_copy)
      {
        SCORE_ASSERT(child.parent() == &root_copy);
      }

      for (int i = 0; i < root.childCount(); i++)
      {
        SCORE_ASSERT(&root.children().at(i) != &root_copy.children().at(i));
        SCORE_ASSERT(
            root.children().at(i).which()
            == root_copy.children().at(i).which());
      }

      SCORE_ASSERT(root_copy.childAt(0).childCount() == 0);
      SCORE_ASSERT(root_copy.childAt(1).childCount() == 1);

      SCORE_ASSERT(
          root_copy.childAt(1).childAt(0).is<Device::AddressSettings>());
      SCORE_ASSERT(
          root_copy.childAt(1).childAt(0).parent() == &root_copy.childAt(1));
    }
  }

  void test_serialize_any()
  {
    auto& anySer = score::anySerializers();
    anySer.emplace(std::string("instanceBounds"), std::make_unique<score::any_serializer_t<ossia::net::instance_bounds>>());
    anySer.emplace(std::string("description"), std::make_unique<score::any_serializer_t<ossia::net::description>>());
    anySer.emplace(std::string("priority"), std::make_unique<score::any_serializer_t<ossia::net::priority>>());
    anySer.emplace(std::string("tags"), std::make_unique<score::any_serializer_t<ossia::net::tags>>());
    anySer.emplace(std::string("refreshRate"), std::make_unique<score::any_serializer_t<ossia::net::refresh_rate>>());
    anySer.emplace(std::string("valueStepSize"), std::make_unique<score::any_serializer_t<ossia::net::value_step_size>>());
    score::testing::MockApplication app;
    ossia::extended_attributes s;
    {
      auto out = DataStreamWriter::unmarshall<ossia::extended_attributes >(DataStreamReader::marshall(s));
    }


    ossia::net::set_tags(s, std::vector<std::string>{"tutu", "titi"});
    {
      auto out = DataStreamWriter::unmarshall<ossia::extended_attributes >(DataStreamReader::marshall(s));
    }
    ossia::net::set_description(s, std::string("something"));
    {
      auto out = DataStreamWriter::unmarshall<ossia::extended_attributes >(DataStreamReader::marshall(s));
    }
    ossia::net::set_priority(s, 1234);
    {
      auto out = DataStreamWriter::unmarshall<ossia::extended_attributes >(DataStreamReader::marshall(s));
    }

    // TODO add more tests
  }

  void test_serialize()
  {
    Device::Node root;
    Device::DeviceSettings dev_base{
        UuidKey<Device::ProtocolFactory>{"85783b8d-454d-4326-a070-9666d2534eff"}, "ADevice", {}};
    auto& dev = root.emplace_back(std::move(dev_base), nullptr);

    Device::Node child(Device::AddressSettings{}, &dev);
    dev.push_back(child);

    score::testing::MockApplication app;
    Device::AddressSettings s;
    for(auto val : {
        ossia::value(State::impulse{}),
        ossia::value(int{}),
        ossia::value(float{}),
        ossia::value(char{}),
        ossia::value(std::string{}),
        ossia::value(State::list_t{}),
        ossia::value(std::array<float,2>{}),
        ossia::value(std::array<float,3>{}),
        ossia::value(std::array<float,4>{})
  })
    {
      s.value = val;
      auto out = DataStreamWriter::unmarshall<Device::AddressSettings>(DataStreamReader::marshall(s));
      QVERIFY(out == s);
    }

    ossia::net::set_tags(s, std::vector<std::string>{"tutu", "titi"});
    ossia::net::set_description(s, std::string("something"));
    ossia::net::set_priority(s, 1234);

    auto out = DataStreamWriter::unmarshall<Device::AddressSettings>(DataStreamReader::marshall(s));
    QVERIFY(out == s);
  }
};

QTEST_MAIN(NodeTest)
#include "NodeTest.moc"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <score/model/tree/TreeNode.hpp>
