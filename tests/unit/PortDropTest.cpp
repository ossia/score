// Unit test: what dragging device explorer nodes onto a port resolves to.
//
// The device explorer serializes a drag as a node list: for each node, its
// address and its data (Device::FreeNode). Dropping a parameter sets the
// port's address and copies its settings. Dropping a device root is legal too
// - a value inlet then receives the whole tree as a map - but the root node
// carries DeviceSettings, not AddressSettings, and used to be rejected
// outright.
//
// The lists are built the way NodeListMimeSerialization builds them, without
// going through the mime round-trip: serializing DeviceSettings needs the
// application's protocol factories.

#include <State/Address.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
// dev:/ { foo (float), group { bar (int) } }
Device::Node makeTree()
{
  Device::DeviceSettings dev;
  dev.name = "dev";
  Device::Node root{dev, nullptr};

  Device::AddressSettings foo;
  foo.name = "foo";
  foo.value = 0.5f;
  root.emplace_back(foo, &root);

  Device::AddressSettings group;
  group.name = "group";
  auto& g = root.emplace_back(group, &root);

  Device::AddressSettings bar;
  bar.name = "bar";
  bar.value = 3;
  g.emplace_back(bar, &g);
  return root;
}

// What MimeReader<Device::NodeList> puts in the drag for these nodes.
Device::FreeNodeList dragOf(std::initializer_list<const Device::Node*> nodes)
{
  Device::FreeNodeList nl;
  for(auto n : nodes)
    nl.emplace_back(Device::address(*n).address, *n);
  return nl;
}

State::AddressAccessor accessor(const char* str)
{
  auto a = State::parseAddressAccessor(str);
  REQUIRE(a.has_value());
  return *a;
}
}

TEST_CASE("A dropped parameter gives its address and settings", "[dataflow][port][drop]")
{
  auto tree = makeTree();
  auto& bar = tree.childAt(1).childAt(0);

  auto res = Device::addressOfDroppedNodes(dragOf({&bar}), {});
  REQUIRE(res);
  CHECK(res->address == accessor("dev:/group/bar"));
  REQUIRE(res->settings);
  CHECK(res->settings->name == "bar");
  CHECK(res->settings->value == ossia::value{3});
}

TEST_CASE("A dropped device root gives the bare device address", "[dataflow][port][drop]")
{
  auto tree = makeTree();

  auto res = Device::addressOfDroppedNodes(dragOf({&tree}), accessor("dev:/foo"));
  REQUIRE(res);
  CHECK(res->address == accessor("dev:/"));
  CHECK(res->address.address.device == "dev");
  CHECK(res->address.address.path.empty());
  // No AddressSettings to copy: the drop is an address change only.
  CHECK(!res->settings);
}

TEST_CASE("Only the first dropped node counts", "[dataflow][port][drop]")
{
  auto tree = makeTree();
  auto& foo = tree.childAt(0);

  auto res = Device::addressOfDroppedNodes(dragOf({&foo, &tree}), {});
  REQUIRE(res);
  CHECK(res->address == accessor("dev:/foo"));
  REQUIRE(res->settings);
  CHECK(res->settings->name == "foo");
}

TEST_CASE("A drop that changes nothing is ignored", "[dataflow][port][drop]")
{
  auto tree = makeTree();
  auto& foo = tree.childAt(0);

  CHECK(!Device::addressOfDroppedNodes({}, {}));
  CHECK(!Device::addressOfDroppedNodes(dragOf({&foo}), accessor("dev:/foo")));
  CHECK(!Device::addressOfDroppedNodes(dragOf({&tree}), accessor("dev:/")));

  // Same node, port carrying an accessor: the address part is what is
  // compared, as before.
  CHECK(!Device::addressOfDroppedNodes(dragOf({&foo}), accessor("dev:/foo@[0]")));

  // A node without a device (the explorer's invisible root) is not an address.
  Device::Node orphan{Device::AddressSettings{}, nullptr};
  Device::FreeNodeList nl{{State::Address{}, orphan}};
  CHECK(!Device::addressOfDroppedNodes(nl, {}));
}
