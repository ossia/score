// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "Utils.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/VariantSerialization.hpp>

using namespace score;
TEST_CASE("serializationTest", "serializationTest")
{
  using namespace Device;
  GIVEN("Empty node")
  {
    Device::Node n{InvisibleRootNode{}, nullptr};
    REQUIRE(n == unmarshall<Device::Node>(marshall<JSONObject>(n)));
    REQUIRE(n == unmarshall<Device::Node>(marshall<DataStream>(n)));
  }
  GIVEN("Device node")
  {
    Device::Node n{
      Device::DeviceSettings{UuidKey<Device::ProtocolFactory>{"31defb61-5f56-4170-91be-b942caa475f6"}, "Foo", QVariant{}},
      nullptr
    };
    REQUIRE(n == unmarshall<Device::Node>(marshall<JSONObject>(n)));
    REQUIRE(n == unmarshall<Device::Node>(marshall<DataStream>(n)));
  }
  GIVEN("Address node")
  {
    Device::Node n{
      Device::AddressSettings{},
      nullptr
    };
    REQUIRE(n == unmarshall<Device::Node>(marshall<JSONObject>(n)));
    REQUIRE(n == unmarshall<Device::Node>(marshall<DataStream>(n)));
  }
}
