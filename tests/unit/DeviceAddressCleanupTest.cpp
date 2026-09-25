// Execution::DocumentPlugin::unregisterDevice queues its port cleanup, so it
// runs after the device - and any other removed since - may be destroyed. The
// match is therefore pointer identity against a snapshot taken while the
// device was alive, never a dereference of what a port still stores.

#include <Execution/DeviceAddresses.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
// destination_t holds a traversal::path alternative: the variant needs it complete
#include <ossia/network/common/path.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/local/local.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>

namespace
{
std::unique_ptr<ossia::net::generic_device> makeDevice(const std::string& name)
{
  return std::make_unique<ossia::net::generic_device>(
      std::make_unique<ossia::net::multiplex_protocol>(), name);
}

ossia::net::parameter_base* addParam(ossia::net::device_base& d, const std::string& path)
{
  auto& n = ossia::net::find_or_create_node(d.get_root_node(), path);
  return n.create_parameter(ossia::val_type::FLOAT);
}
}

TEST_CASE(
    "deviceAddresses collects a device's nodes and parameters", "[execution][devices]")
{
  auto dev = makeDevice("dev");
  auto* a = addParam(*dev, "/a");
  auto* b = addParam(*dev, "/deep/b");
  REQUIRE(a);
  REQUIRE(b);

  const auto owned = Execution::deviceAddresses(*dev);

  CHECK(owned.contains(&dev->get_root_node()));
  CHECK(owned.contains(a));
  CHECK(owned.contains(b));
  // intermediate nodes are in there too: a port may address a node, not only
  // a parameter
  CHECK(owned.contains(&ossia::net::find_or_create_node(dev->get_root_node(), "/deep")));
}

TEST_CASE(
    "addressBelongsTo matches only the device that owns the address",
    "[execution][devices]")
{
  auto mine = makeDevice("mine");
  auto theirs = makeDevice("theirs");
  auto* myParam = addParam(*mine, "/value");
  auto* theirParam = addParam(*theirs, "/value");
  REQUIRE(myParam);
  REQUIRE(theirParam);

  const auto owned = Execution::deviceAddresses(*mine);

  CHECK(Execution::addressBelongsTo(ossia::destination_t{myParam}, owned));
  CHECK(
      Execution::addressBelongsTo(ossia::destination_t{&mine->get_root_node()}, owned));

  CHECK_FALSE(Execution::addressBelongsTo(ossia::destination_t{theirParam}, owned));
  CHECK_FALSE(Execution::addressBelongsTo(
      ossia::destination_t{&theirs->get_root_node()}, owned));

  // an address a port never got: no match, no crash
  CHECK_FALSE(Execution::addressBelongsTo(ossia::destination_t{}, owned));
}

TEST_CASE(
    "addressBelongsTo never dereferences the address it is given",
    "[execution][devices]")
{
  auto live = makeDevice("live");
  auto* liveParam = addParam(*live, "/value");
  REQUIRE(liveParam);

  // A device removed just before this one, whose ports nothing has cleaned up
  // yet: the raw pointer a port still stores now dangles. Only its value is
  // used below - it is never read through.
  ossia::net::parameter_base* dangling{};
  ossia::net::node_base* danglingNode{};
  {
    auto gone = makeDevice("gone");
    dangling = addParam(*gone, "/value");
    danglingNode = &gone->get_root_node();
    REQUIRE(dangling);
  }

  const auto owned = Execution::deviceAddresses(*live);

  // The regression. Against the old code these two dereferenced freed memory.
  CHECK_FALSE(Execution::addressBelongsTo(ossia::destination_t{dangling}, owned));
  CHECK_FALSE(Execution::addressBelongsTo(ossia::destination_t{danglingNode}, owned));

  // Same property, stated so that it cannot pass by luck: a pointer that is
  // not an object at all. An identity lookup is fine; a dereference faults.
  auto* bogus = reinterpret_cast<ossia::net::parameter_base*>(std::uintptr_t{0xD15EA5E});
  CHECK_FALSE(Execution::addressBelongsTo(ossia::destination_t{bogus}, owned));

  // and the live device still answers correctly afterwards
  CHECK(Execution::addressBelongsTo(ossia::destination_t{liveParam}, owned));
}

namespace
{
struct test_node final : ossia::graph_node
{
  std::string label() const noexcept override { return "test"; }
};
}

TEST_CASE(
    "clearAddresses resets the removed device's ports and nothing else",
    "[execution][devices]")
{
  auto removed = makeDevice("removed");
  auto kept = makeDevice("kept");
  auto* gone = addParam(*removed, "/gone");
  auto* stays = addParam(*kept, "/stays");

  // graph_node owns its ports and deletes them
  test_node node;
  auto* in_removed = new ossia::value_inlet;
  auto* in_kept = new ossia::value_inlet;
  auto* out_removed = new ossia::value_outlet;
  auto* out_unaddressed = new ossia::value_outlet;
  in_removed->address = gone;
  in_kept->address = stays;
  out_removed->address
      = &ossia::net::find_or_create_node(removed->get_root_node(), "/n");

  node.root_inputs().push_back(in_removed);
  node.root_inputs().push_back(in_kept);
  node.root_outputs().push_back(out_removed);
  node.root_outputs().push_back(out_unaddressed);

  const auto owned = Execution::deviceAddresses(*removed);
  ossia::graph_node* nodes[] = {&node};

  // The snapshot outlives the device: this is the order the queued cleanup
  // really runs in.
  removed.reset();
  Execution::clearAddresses(nodes, owned);

  CHECK(in_removed->address == ossia::destination_t{});
  CHECK(out_removed->address == ossia::destination_t{});
  CHECK(out_unaddressed->address == ossia::destination_t{});

  // The other device is still addressed, by the same pointer.
  REQUIRE(in_kept->address.target<ossia::net::parameter_base*>());
  CHECK(*in_kept->address.target<ossia::net::parameter_base*>() == stays);
}
