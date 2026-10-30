// Pattern combiner: the output of every mode over float, list and map inputs.

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/value/format_value.hpp>

#include <AvndProcesses/Combiner.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <vector>

template <>
struct Catch::StringMaker<ossia::value>
{
  static std::string convert(const ossia::value& v) { return fmt::format("{}", v); }
};

namespace
{
using Combiner = avnd_tools::PatternCombiner;
using M = decltype(Combiner{}.inputs.mode)::enum_type;
using list = std::vector<ossia::value>;

struct Fixture
{
  ossia::net::generic_device dev{"dev"};
  Combiner c;

  Fixture()
  {
    // The pattern is resolved by the selector at execution; the combiner only
    // reads the nodes it matched.
    c.m_paths.push_back(*ossia::traversal::make_path("/*"));
  }

  void add(std::string_view name, ossia::val_type t, ossia::value v)
  {
    auto& n = ossia::net::create_node(dev.get_root_node(), name);
    n.create_parameter(t)->set_value(std::move(v));
    c.roots.push_back(&n);
  }

  ossia::value run(M m)
  {
    c.inputs.mode.value = m;
    c.outputs.output.value = ossia::value{};
    c();
    return c.outputs.output.value;
  }
};
}

TEST_CASE("scalars", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::FLOAT, -1.f);
  f.add("b", ossia::val_type::FLOAT, -3.f);

  CHECK(f.run(M::List) == ossia::value{list{-1.f, -3.f}});
  CHECK(f.run(M::Average) == ossia::value{-2.f});
  CHECK(f.run(M::Sum) == ossia::value{-4.f});
  // Min and max start from their identity, not from zero
  CHECK(f.run(M::Min) == ossia::value{-3.f});
  CHECK(f.run(M::Max) == ossia::value{-1.f});
  CHECK(
      f.run(M::Map)
      == ossia::value{ossia::value_map_type{{"dev:/a", -1.f}, {"dev:/b", -3.f}}});
}

TEST_CASE("mixed types reduce as numbers", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::INT, 4);
  f.add("b", ossia::val_type::FLOAT, 2.f);

  CHECK(f.run(M::Average) == ossia::value{3.f});
  CHECK(f.run(M::Min) == ossia::value{2.f});
  CHECK(
      f.run(M::Map) == ossia::value{ossia::value_map_type{{"dev:/a", 4}, {"dev:/b", 2.f}}});
}

TEST_CASE("map keys are the full address", "[pattern_combiner]")
{
  Fixture f;
  // Same name under two parents: only the full address tells them apart
  auto& p1 = ossia::net::create_node(f.dev.get_root_node(), "p1/x");
  auto& p2 = ossia::net::create_node(f.dev.get_root_node(), "p2/x");
  p1.create_parameter(ossia::val_type::FLOAT)->set_value(1.f);
  p2.create_parameter(ossia::val_type::FLOAT)->set_value(2.f);
  f.c.roots = {&p1, &p2};

  CHECK(
      f.run(M::Map)
      == ossia::value{ossia::value_map_type{{"dev:/p1/x", 1.f}, {"dev:/p2/x", 2.f}}});
}

TEST_CASE("lists reduce element-wise", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::LIST, list{1.f, 4.f});
  f.add("b", ossia::val_type::LIST, list{3.f, 2.f});

  CHECK(f.run(M::Average) == ossia::value{list{2.f, 3.f}});
  CHECK(f.run(M::Sum) == ossia::value{list{4.f, 6.f}});
  CHECK(f.run(M::Min) == ossia::value{list{1.f, 2.f}});
  CHECK(f.run(M::Max) == ossia::value{list{3.f, 4.f}});
}

TEST_CASE("maps reduce key by key", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::MAP, ossia::value_map_type{{"x", 1.f}, {"y", 5.f}});
  // "y" is missing here: it is reduced over the one input that has it
  f.add("b", ossia::val_type::MAP, ossia::value_map_type{{"x", 3.f}, {"z", -2.f}});

  CHECK(
      f.run(M::Average)
      == ossia::value{ossia::value_map_type{{"x", 2.f}, {"y", 5.f}, {"z", -2.f}}});
  CHECK(
      f.run(M::Sum)
      == ossia::value{ossia::value_map_type{{"x", 4.f}, {"y", 5.f}, {"z", -2.f}}});
  CHECK(
      f.run(M::Min)
      == ossia::value{ossia::value_map_type{{"x", 1.f}, {"y", 5.f}, {"z", -2.f}}});
  CHECK(
      f.run(M::Max)
      == ossia::value{ossia::value_map_type{{"x", 3.f}, {"y", 5.f}, {"z", -2.f}}});
}
