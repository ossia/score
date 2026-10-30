// Pattern combiner: the output of every mode over float, vector, list, map
// and string inputs.

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/value/format_value.hpp>

#include <AvndProcesses/Combiner.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string>
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

  void set(std::string_view name, ossia::value v)
  {
    ossia::net::find_node(dev.get_root_node(), name)->get_parameter()->set_value(std::move(v));
  }

  //! One tick; nothing when the combiner sent nothing. The binding clears
  //! the output before each tick.
  std::optional<ossia::value> tick(M m)
  {
    c.inputs.mode.value = m;
    c.outputs.output.value.reset();
    c();
    return c.outputs.output.value;
  }

  ossia::value run(M m) { return tick(m).value_or(ossia::value{}); }
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

TEST_CASE("argmax and argmin give the address", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::FLOAT, -1.f);
  f.add("b", ossia::val_type::INT, 5);
  f.add("c", ossia::val_type::FLOAT, 5.f);
  f.add("s", ossia::val_type::STRING, std::string{"9"});

  // A tie goes to the first by address; strings are not ranked
  CHECK(f.run(M::ArgMax) == ossia::value{std::string{"dev:/b"}});
  CHECK(f.run(M::ArgMin) == ossia::value{std::string{"dev:/a"}});
}

TEST_CASE("vectors are ranked by their length", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::VEC2F, ossia::vec2f{3.f, 4.f});
  f.add("b", ossia::val_type::VEC2F, ossia::vec2f{0.f, 4.5f});
  f.add("c", ossia::val_type::VEC2F, ossia::vec2f{-6.f, 0.f});

  CHECK(f.run(M::ArgMax) == ossia::value{std::string{"dev:/c"}});
  CHECK(f.run(M::ArgMin) == ossia::value{std::string{"dev:/b"}});
  CHECK(
      f.run(M::Sort)
      == ossia::value{ossia::value_map_type{
          {"dev:/b", ossia::vec2f{0.f, 4.5f}},
          {"dev:/a", ossia::vec2f{3.f, 4.f}},
          {"dev:/c", ossia::vec2f{-6.f, 0.f}}}});
}

TEST_CASE("sort and top N", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::FLOAT, 3.f);
  f.add("b", ossia::val_type::FLOAT, 1.f);
  f.add("c", ossia::val_type::FLOAT, 2.f);
  f.add("s", ossia::val_type::STRING, std::string{"x"});

  CHECK(
      f.run(M::Sort)
      == ossia::value{
          ossia::value_map_type{{"dev:/b", 1.f}, {"dev:/c", 2.f}, {"dev:/a", 3.f}}});
  f.c.inputs.n.value = 2;
  CHECK(
      f.run(M::TopN) == ossia::value{ossia::value_map_type{{"dev:/a", 3.f}, {"dev:/c", 2.f}}});
  // More than there are: all of them
  f.c.inputs.n.value = 10;
  CHECK(f.run(M::TopN).get<ossia::value_map_type>().size() == 3);
}

TEST_CASE("median, range and standard deviation", "[pattern_combiner]")
{
  Fixture f;
  for(auto [name, v] : {std::pair{"a", 2.f}, {"b", 4.f}, {"c", 4.f}, {"d", 4.f},
                        {"e", 5.f}, {"f", 5.f}, {"g", 7.f}, {"h", 9.f}})
    f.add(name, ossia::val_type::FLOAT, v);

  CHECK(f.run(M::Median) == ossia::value{4.5f});
  CHECK(f.run(M::Range) == ossia::value{7.f});
  CHECK(f.run(M::StdDev) == ossia::value{2.f});

  f.set("h", 100.f);
  // One outlier moves the average a lot, the median not
  CHECK(f.run(M::Median) == ossia::value{4.5f});
}

TEST_CASE("statistics go element by element", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::VEC2F, ossia::vec2f{1.f, 10.f});
  f.add("b", ossia::val_type::VEC2F, ossia::vec2f{9.f, 30.f});
  f.add("c", ossia::val_type::VEC2F, ossia::vec2f{3.f, 20.f});
  CHECK(f.run(M::Median) == ossia::value{ossia::vec2f{3.f, 20.f}});
  CHECK(f.run(M::Range) == ossia::value{ossia::vec2f{8.f, 20.f}});

  Fixture m;
  m.add("a", ossia::val_type::MAP, ossia::value_map_type{{"x", 1.f}, {"y", 5.f}});
  m.add("b", ossia::val_type::MAP, ossia::value_map_type{{"x", 7.f}});
  CHECK(
      m.run(M::Range) == ossia::value{ossia::value_map_type{{"x", 6.f}, {"y", 0.f}}});

  Fixture l;
  l.add("a", ossia::val_type::LIST, list{1.f, 0.f});
  l.add("b", ossia::val_type::LIST, list{3.f, 1.f});
  l.c.inputs.test.value = decltype(l.c.inputs.test)::Above;
  CHECK(l.run(M::Count) == ossia::value{list{2, 1}});
}

TEST_CASE("count, any and all: above with hysteresis", "[pattern_combiner]")
{
  Fixture f;
  f.c.inputs.test.value = decltype(f.c.inputs.test)::Above;
  f.c.inputs.min.value = 0.4f;
  f.c.inputs.max.value = 0.6f;
  f.add("a", ossia::val_type::FLOAT, 0.5f);
  f.add("b", ossia::val_type::FLOAT, 0.7f);
  f.add("c", ossia::val_type::FLOAT, 0.1f);

  // a is in the band and was never above Max: off
  CHECK(f.run(M::Count) == ossia::value{1});

  // b falls into the band: stays on; a stays off
  f.set("b", 0.5f);
  CHECK(f.run(M::Count) == ossia::value{1});
  CHECK(f.run(M::Any) == ossia::value{true});
  CHECK(f.run(M::All) == ossia::value{false});

  // Below Min: off
  f.set("b", 0.3f);
  CHECK(f.run(M::Count) == ossia::value{0});
  CHECK(f.run(M::Any) == ossia::value{false});

  f.set("a", 0.9f);
  f.set("b", 0.9f);
  f.set("c", 0.9f);
  CHECK(f.run(M::All) == ossia::value{true});
}

TEST_CASE("count: below, inside, outside", "[pattern_combiner]")
{
  Fixture f;
  f.c.inputs.min.value = 0.f;
  f.c.inputs.max.value = 1.f;
  f.add("a", ossia::val_type::FLOAT, -1.f);
  f.add("b", ossia::val_type::FLOAT, 0.5f);
  f.add("c", ossia::val_type::FLOAT, 1.f);
  f.add("d", ossia::val_type::FLOAT, 2.f);

  using T = decltype(f.c.inputs.test);
  f.c.inputs.test.value = T::Inside;
  CHECK(f.run(M::Count) == ossia::value{2});
  f.c.inputs.test.value = T::Outside;
  CHECK(f.run(M::Count) == ossia::value{2});
  f.c.inputs.test.value = T::Below;
  CHECK(f.run(M::Count) == ossia::value{1});
}

TEST_CASE("changed sends only what changed", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::FLOAT, 1.f);
  f.add("b", ossia::val_type::FLOAT, 2.f);

  // First tick: everything
  CHECK(
      f.tick(M::Changed)
      == ossia::value{ossia::value_map_type{{"dev:/a", 1.f}, {"dev:/b", 2.f}}});
  // Nothing changed: nothing goes out
  CHECK(!f.tick(M::Changed));

  f.set("b", 3.f);
  CHECK(f.tick(M::Changed) == ossia::value{ossia::value_map_type{{"dev:/b", 3.f}}});
  CHECK(!f.tick(M::Changed));

  // A new input counts as changed
  f.add("c", ossia::val_type::FLOAT, 0.f);
  CHECK(f.tick(M::Changed) == ossia::value{ossia::value_map_type{{"dev:/c", 0.f}}});

  // Back from another mode: everything again
  f.tick(M::List);
  CHECK(f.tick(M::Changed)->get<ossia::value_map_type>().size() == 3);
}

TEST_CASE("vote", "[pattern_combiner]")
{
  Fixture f;
  f.add("a", ossia::val_type::STRING, std::string{"left"});
  f.add("b", ossia::val_type::STRING, std::string{"right"});
  f.add("c", ossia::val_type::STRING, std::string{"right"});
  f.add("d", ossia::val_type::IMPULSE, ossia::impulse{});
  CHECK(f.run(M::Vote) == ossia::value{std::string{"right"}});

  // A tie: the first by address
  f.set("c", std::string{"left"});
  f.set("a", std::string{"up"});
  f.set("b", std::string{"down"});
  CHECK(f.run(M::Vote) == ossia::value{std::string{"up"}});
}

namespace
{
constexpr float fnan = std::numeric_limits<float>::quiet_NaN();
bool is_nan(const ossia::value& v)
{
  auto f = v.target<float>();
  return f && std::isnan(*f);
}
}

TEST_CASE("NaN is left out of every reduction and ranking", "[pattern_combiner][edge]")
{
  Fixture f;
  // NaN first: it must not win ArgMax by being compared to nothing
  f.add("a", ossia::val_type::FLOAT, fnan);
  f.add("b", ossia::val_type::FLOAT, 1.f);
  f.add("c", ossia::val_type::FLOAT, fnan);
  f.add("d", ossia::val_type::FLOAT, 3.f);
  f.add("e", ossia::val_type::FLOAT, 2.f);

  CHECK(f.run(M::Average) == ossia::value{2.f});
  CHECK(f.run(M::Sum) == ossia::value{6.f});
  CHECK(f.run(M::Min) == ossia::value{1.f});
  CHECK(f.run(M::Max) == ossia::value{3.f});
  CHECK(f.run(M::Median) == ossia::value{2.f});
  CHECK(f.run(M::Range) == ossia::value{2.f});
  CHECK(f.run(M::ArgMax) == ossia::value{std::string{"dev:/d"}});
  CHECK(f.run(M::ArgMin) == ossia::value{std::string{"dev:/b"}});
  CHECK(
      f.run(M::Sort)
      == ossia::value{
          ossia::value_map_type{{"dev:/b", 1.f}, {"dev:/e", 2.f}, {"dev:/d", 3.f}}});

  // NaN passes no test
  f.c.inputs.test.value = decltype(f.c.inputs.test)::Outside;
  CHECK(f.run(M::Count) == ossia::value{3});
}

TEST_CASE("many NaN do not break the median", "[pattern_combiner][edge]")
{
  Fixture f;
  // Enough values that the selection partitions, NaN all over them
  for(int i = 0; i < 301; i++)
    f.add(
        "n" + std::to_string(i), ossia::val_type::FLOAT,
        i % 3 == 0 ? fnan : float(i % 100));
  const auto m = f.run(M::Median);
  REQUIRE(m.get_type() == ossia::val_type::FLOAT);
  CHECK(!std::isnan(m.get<float>()));
  CHECK(f.run(M::TopN).get<ossia::value_map_type>().size() == 3);
}

TEST_CASE("only NaN: nothing to reduce", "[pattern_combiner][edge]")
{
  Fixture f;
  f.add("a", ossia::val_type::FLOAT, fnan);
  f.add("b", ossia::val_type::FLOAT, fnan);
  for(auto m : {M::Average, M::Sum, M::Min, M::Max, M::Median, M::Range, M::StdDev,
                M::ArgMax, M::ArgMin})
  {
    CAPTURE(int(m));
    CHECK(!f.tick(m));
  }
  CHECK(f.run(M::Sort).get<ossia::value_map_type>().empty());
  CHECK(f.run(M::Count) == ossia::value{0});
  // A sensor stuck on NaN has not changed
  CHECK(f.tick(M::Changed));
  CHECK(!f.tick(M::Changed));
}

TEST_CASE("strings and impulses are not numbers", "[pattern_combiner][edge]")
{
  Fixture f;
  f.add("a", ossia::val_type::FLOAT, 2.f);
  f.add("b", ossia::val_type::STRING, std::string{"x"});
  f.add("c", ossia::val_type::INT, 4);
  f.add("d", ossia::val_type::IMPULSE, ossia::impulse{});

  CHECK(f.run(M::Average) == ossia::value{3.f});
  CHECK(f.run(M::Sum) == ossia::value{6.f});
  CHECK(f.run(M::Min) == ossia::value{2.f});
  f.c.inputs.test.value = decltype(f.c.inputs.test)::Above;
  CHECK(f.run(M::Count) == ossia::value{2});
  CHECK(f.run(M::All) == ossia::value{false});
  CHECK(f.run(M::List).get<list>().size() == 4);

  Fixture s;
  s.add("a", ossia::val_type::STRING, std::string{"x"});
  s.add("b", ossia::val_type::STRING, std::string{"y"});
  CHECK(!s.tick(M::Average));
  CHECK(!s.tick(M::ArgMax));
  CHECK(s.run(M::Vote) == ossia::value{std::string{"x"}});

  Fixture i;
  i.add("a", ossia::val_type::IMPULSE, ossia::impulse{});
  CHECK(!i.tick(M::Average));
  CHECK(!i.tick(M::Vote));
  CHECK(i.run(M::Count) == ossia::value{0});
}

TEST_CASE("nothing matched: nothing goes out", "[pattern_combiner][edge]")
{
  for(auto m : {M::List, M::Average, M::Sum, M::Min, M::Max, M::Map, M::ArgMax,
                M::ArgMin, M::Median, M::Count, M::Any, M::All, M::Range, M::StdDev,
                M::Changed, M::Vote, M::Sort, M::TopN})
  {
    CAPTURE(int(m));
    Fixture f;
    CHECK(!f.tick(m));

    // A node without a parameter is not an input
    auto& n = ossia::net::create_node(f.dev.get_root_node(), "empty");
    f.c.roots.push_back(&n);
    CHECK(!f.tick(m));

    // No pattern
    Fixture g;
    g.add("a", ossia::val_type::FLOAT, 1.f);
    g.c.m_paths.clear();
    CHECK(!g.tick(m));
  }
}

TEST_CASE("bools are 0 and 1", "[pattern_combiner][edge]")
{
  Fixture f;
  f.add("a", ossia::val_type::BOOL, true);
  f.add("b", ossia::val_type::BOOL, false);
  f.add("c", ossia::val_type::BOOL, true);
  CHECK(f.run(M::Sum) == ossia::value{2.f});
  f.c.inputs.test.value = decltype(f.c.inputs.test)::Above;
  CHECK(f.run(M::Count) == ossia::value{2});
  CHECK(f.run(M::Vote) == ossia::value{true});
}

TEST_CASE("lists of different lengths go element by element", "[pattern_combiner][edge]")
{
  Fixture f;
  f.add("a", ossia::val_type::LIST, list{1.f, 2.f, 3.f});
  f.add("b", ossia::val_type::LIST, list{3.f});
  f.add("c", ossia::val_type::LIST, list{});
  CHECK(f.run(M::Average) == ossia::value{list{2.f, 2.f, 3.f}});
  CHECK(f.run(M::Max) == ossia::value{list{3.f, 2.f, 3.f}});

  // An element that is no number is left out; one that is never one is NaN
  Fixture g;
  g.add("a", ossia::val_type::LIST, list{1.f, std::string{"x"}, std::string{"y"}});
  g.add("b", ossia::val_type::LIST, list{3, 5.f, std::string{"z"}});
  const auto r = g.run(M::Average).get<list>();
  REQUIRE(r.size() == 3);
  CHECK(r[0] == ossia::value{2.f});
  CHECK(r[1] == ossia::value{5.f});
  CHECK(is_nan(r[2]));
}

TEST_CASE("vectors with NaN elements", "[pattern_combiner][edge]")
{
  Fixture f;
  f.add("a", ossia::val_type::VEC2F, ossia::vec2f{1.f, fnan});
  f.add("b", ossia::val_type::VEC2F, ossia::vec2f{3.f, fnan});
  const auto r = f.run(M::Average);
  REQUIRE(r.get_type() == ossia::val_type::VEC2F);
  CHECK(r.get<ossia::vec2f>()[0] == 2.f);
  CHECK(std::isnan(r.get<ossia::vec2f>()[1]));
  // Ranked by the length of the elements that are numbers, as lists are
  CHECK(f.run(M::ArgMax) == ossia::value{std::string{"dev:/b"}});
  f.add("c", ossia::val_type::VEC2F, ossia::vec2f{fnan, fnan});
  CHECK(f.run(M::Sort).get<ossia::value_map_type>().size() == 2);
}

TEST_CASE("maps with keys in other orders, and values that are no numbers",
          "[pattern_combiner][edge]")
{
  Fixture f;
  f.add("a", ossia::val_type::MAP,
        ossia::value_map_type{{"x", 1.f}, {"y", 2.f}, {"s", std::string{"no"}}});
  f.add("b", ossia::val_type::MAP, ossia::value_map_type{{"y", 4.f}, {"x", 3.f}});
  f.add("c", ossia::val_type::MAP, ossia::value_map_type{});
  // "s" reduces to nothing: it is not in the result
  CHECK(
      f.run(M::Average) == ossia::value{ossia::value_map_type{{"x", 2.f}, {"y", 3.f}}});
  f.c.inputs.test.value = decltype(f.c.inputs.test)::Above;
  CHECK(
      f.run(M::Count)
      == ossia::value{ossia::value_map_type{{"x", 2}, {"y", 2}, {"s", 0}}});
}

TEST_CASE("min above max is the same range", "[pattern_combiner][edge]")
{
  Fixture f;
  f.c.inputs.min.value = 1.f;
  f.c.inputs.max.value = 0.f;
  f.c.inputs.test.value = decltype(f.c.inputs.test)::Inside;
  f.add("a", ossia::val_type::FLOAT, 0.5f);
  f.add("b", ossia::val_type::FLOAT, 2.f);
  CHECK(f.run(M::Count) == ossia::value{1});
}

TEST_CASE("top N below 1 keeps one", "[pattern_combiner][edge]")
{
  Fixture f;
  f.add("a", ossia::val_type::FLOAT, 1.f);
  f.add("b", ossia::val_type::FLOAT, 2.f);
  // A cable can set it outside of its range
  f.c.inputs.n.value = 0;
  CHECK(f.run(M::TopN) == ossia::value{ossia::value_map_type{{"dev:/b", 2.f}}});
  f.c.inputs.n.value = -5;
  CHECK(f.run(M::TopN).get<ossia::value_map_type>().size() == 1);
}

TEST_CASE("vote tells values apart by type and content", "[pattern_combiner][edge]")
{
  Fixture f;
  f.add("a", ossia::val_type::INT, 1);
  f.add("b", ossia::val_type::FLOAT, 1.f);
  f.add("c", ossia::val_type::FLOAT, 1.f);
  // 1 and 1.0 are two choices
  CHECK(f.run(M::Vote) == ossia::value{1.f});

  Fixture l;
  l.add("a", ossia::val_type::LIST, list{1, 2});
  l.add("b", ossia::val_type::LIST, list{2, 1});
  l.add("c", ossia::val_type::LIST, list{2, 1});
  // Same size, so same hash: still two choices
  CHECK(l.run(M::Vote) == ossia::value{list{2, 1}});

  Fixture v;
  v.add("a", ossia::val_type::VEC3F, ossia::vec3f{1.f, 2.f, 3.f});
  v.add("b", ossia::val_type::VEC3F, ossia::vec3f{0.f, 0.f, 0.f});
  v.add("c", ossia::val_type::VEC3F, ossia::vec3f{0.f, 0.f, 0.f});
  CHECK(v.run(M::Vote) == ossia::value{ossia::vec3f{0.f, 0.f, 0.f}});
}

TEST_CASE("vote over many distinct values", "[pattern_combiner][edge]")
{
  Fixture f;
  for(int i = 0; i < 2000; i++)
    f.add("n" + std::to_string(i), ossia::val_type::FLOAT, float(i));
  // Every value once: the first by address
  CHECK(f.run(M::Vote) == ossia::value{0.f});
  f.set("n1500", 1234.f);
  f.set("n1700", 1234.f);
  CHECK(f.run(M::Vote) == ossia::value{1234.f});
}

TEST_CASE("changed follows inputs that move around", "[pattern_combiner][edge]")
{
  Fixture f;
  for(int i = 0; i < 500; i++)
    f.add("n" + std::to_string(i), ossia::val_type::FLOAT, float(i));
  CHECK(f.tick(M::Changed)->get<ossia::value_map_type>().size() == 500);

  // The same inputs in another order: none of them changed
  std::reverse(f.c.roots.begin(), f.c.roots.end());
  CHECK(!f.tick(M::Changed));

  // One input gone, one changed
  f.c.roots.pop_back();
  f.set("n42", -1.f);
  CHECK(f.tick(M::Changed) == ossia::value{ossia::value_map_type{{"dev:/n42", -1.f}}});
}

TEST_CASE("the counting state starts over when the inputs change",
          "[pattern_combiner][edge]")
{
  Fixture f;
  f.c.inputs.test.value = decltype(f.c.inputs.test)::Above;
  f.c.inputs.min.value = 0.f;
  f.c.inputs.max.value = 1.f;
  f.add("a", ossia::val_type::FLOAT, 2.f);
  CHECK(f.run(M::Count) == ossia::value{1});
  f.set("a", 0.5f);
  CHECK(f.run(M::Count) == ossia::value{1}); // held by the hysteresis

  // Another input in its place: it never went above Max
  f.c.roots.clear();
  f.add("b", ossia::val_type::FLOAT, 0.5f);
  CHECK(f.run(M::Count) == ossia::value{0});
}

TEST_CASE("every mode, tick after tick, from mode to mode", "[pattern_combiner][edge]")
{
  // The values a mode moves or swaps away are gathered again on the next tick
  Fixture f;
  f.add("a", ossia::val_type::FLOAT, 1.f);
  f.add("b", ossia::val_type::FLOAT, 2.f);
  const M all[]{M::List, M::Average, M::Sum, M::Min, M::Max, M::Map, M::ArgMax,
                M::ArgMin, M::Median, M::Count, M::Any, M::All, M::Range, M::StdDev,
                M::Changed, M::Vote, M::Sort, M::TopN};
  for(int round = 0; round < 3; round++)
    for(auto m : all)
      for(auto m2 : {m, M::List, M::Average})
      {
        CAPTURE(round, int(m), int(m2));
        f.tick(m);
        if(m2 == M::List)
          CHECK(f.run(m2) == ossia::value{list{1.f, 2.f}});
        else if(m2 == M::Average)
          CHECK(f.run(m2) == ossia::value{1.5f});
      }
}
