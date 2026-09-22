#include <ossia/dataflow/value_port.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value.hpp>

#include <catch2/catch_all.hpp>

namespace
{
auto cable = [](ossia::value_port& src, ossia::value_port& snk, ossia::value v) {
  src.write_value(std::move(v), 0);
  snk.add_port_values(src);
};
}

TEST_CASE("A vector domain answers a scalar min/max", "[domain]")
{
  auto d = ossia::make_domain(ossia::vec3f{0.f, 0.f, 0.f}, ossia::vec3f{1.f, 1.f, 360.f});
  auto [lo, hi] = ossia::get_float_minmax(d);
  REQUIRE(lo);
  REQUIRE(hi);
  CHECK(*lo == 0.f);
  // "the widest any component takes" -- component 2 decides for all three.
  CHECK(*hi == 360.f);
}

TEST_CASE("vec3->vec3 with non-uniform per-component ranges", "[domain]")
{
  ossia::value_port src, snk;
  src.type = ossia::val_type::VEC3F;
  snk.type = ossia::val_type::VEC3F;

  // Same shape on both sides: x and y in [0,1], z in [0,360].
  src.domain = ossia::make_domain(
      ossia::vec3f{0.f, 0.f, 0.f}, ossia::vec3f{1.f, 1.f, 360.f});
  snk.domain = ossia::make_domain(
      ossia::vec3f{-1.f, -1.f, 0.f}, ossia::vec3f{1.f, 1.f, 360.f});

  cable(src, snk, ossia::vec3f{1.f, 1.f, 360.f});

  REQUIRE(snk.get_data().size() == 1);
  INFO("got " << ossia::value_to_pretty_string(snk.get_data()[0].value));
  // The source value is already the top of every component's range, and the
  // sink's top is identical: nothing should move.
  CHECK(snk.get_data()[0].value == ossia::value{ossia::vec3f{1.f, 1.f, 360.f}});
}

TEST_CASE("vec3->vec3 with identical uniform ranges is a no-op", "[domain]")
{
  ossia::value_port src, snk;
  src.type = ossia::val_type::VEC3F;
  snk.type = ossia::val_type::VEC3F;
  src.domain = ossia::make_domain(ossia::vec3f{0.f, 0.f, 0.f}, ossia::vec3f{1.f, 1.f, 1.f});
  snk.domain = ossia::make_domain(ossia::vec3f{0.f, 0.f, 0.f}, ossia::vec3f{1.f, 1.f, 1.f});

  cable(src, snk, ossia::vec3f{0.25f, 0.5f, 0.75f});
  REQUIRE(snk.get_data().size() == 1);
  CHECK(snk.get_data()[0].value == ossia::value{ossia::vec3f{0.25f, 0.5f, 0.75f}});
}

#include <ossia/network/common/value_mapping.hpp>

// The address path (adapt_value -> map_value) for the very same domains.
TEST_CASE("map_value on the same non-uniform vec3 domains", "[domain]")
{
  auto src = ossia::make_domain(
      ossia::vec3f{0.f, 0.f, 0.f}, ossia::vec3f{1.f, 1.f, 360.f});
  auto snk = ossia::make_domain(
      ossia::vec3f{-1.f, -1.f, 0.f}, ossia::vec3f{1.f, 1.f, 360.f});

  ossia::value v{ossia::vec3f{1.f, 1.f, 360.f}};
  ossia::map_value(v, {}, src, snk);
  INFO("got " << ossia::value_to_pretty_string(v));
  CHECK(v == ossia::value{ossia::vec3f{1.f, 1.f, 360.f}});
}
