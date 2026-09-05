// The ossia binding deduces a port's unit from the shape of its value. Those
// concepts nest, so each overload has to exclude the wider ones.

#include <ossia/dataflow/port.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <avnd/introspection/port.hpp>

#include <avnd/binding/ossia/port_setup.hpp>
#include <halp/controls.hpp>

#include <catch2/catch_all.hpp>

namespace
{
template <typename Field>
ossia::value_port port_for()
{
  ossia::value_port p;
  oscr::setup_value_port::setup_port<Field>(p);
  return p;
}

// As the address panel spells it.
std::string unit_of(const ossia::value_port& p)
{
  if(auto u = p.type.target<ossia::unit_t>())
    return std::string(ossia::get_pretty_unit_text(*u));
  return "<not a unit>";
}

ossia::val_type type_of(const ossia::value_port& p)
{
  auto t = p.type.target<ossia::val_type>();
  REQUIRE(t);
  return *t;
}
}

TEST_CASE("A port's unit follows the arity of its value", "[avnd][port][unit]")
{
  SECTION("two components are a 2D position")
  {
    auto p = port_for<halp::xy_spinboxes_f32<"P", halp::range{0., 1., 0.}>>();
    CHECK(unit_of(p) == "position.cart2D");
  }

  SECTION("three components are a 3D position")
  {
    auto p = port_for<halp::xyz_spinboxes_f32<"P", halp::range{0., 1., 0.}>>();
    CHECK(unit_of(p) == "position.cart3D");
  }

  SECTION("four components have no position unit, only their arity")
  {
    struct xyzw_port
    {
      static consteval auto name() { return "P"; }
      struct
      {
        float x, y, z, w;
      } value;
    };
    auto p = port_for<xyzw_port>();
    CHECK(p.type.target<ossia::unit_t>() == nullptr);
    CHECK(type_of(p) == ossia::val_type::VEC4F);
  }

  SECTION("an xy pad is still a 2D position")
  {
    auto p = port_for<halp::xy_pad_f32<"P", halp::range{0., 1., 0.}>>();
    CHECK(unit_of(p) == "position.cart2D");
  }

  SECTION("scalars are untouched")
  {
    auto p = port_for<halp::hslider_f32<"F", halp::range{0., 1., 0.}>>();
    CHECK(type_of(p) == ossia::val_type::FLOAT);
  }
}

TEST_CASE("A colour port's unit follows its arity too", "[avnd][port][unit]")
{
  // rgba_value satisfies rgb_value, exactly as xyzw does xy.
  SECTION("three components are rgb")
  {
    struct rgb_port
    {
      static consteval auto name() { return "C"; }
      struct
      {
        float r, g, b;
      } value;
    };
    CHECK(unit_of(port_for<rgb_port>()) == "color.rgb");
  }

  SECTION("four components are rgba")
  {
    struct rgba_port
    {
      static consteval auto name() { return "C"; }
      struct
      {
        float r, g, b, a;
      } value;
    };
    CHECK(unit_of(port_for<rgba_port>()) == "color.rgba");
  }
}

TEST_CASE("A unit a process declares itself", "[avnd][port][unit]")
{
  SECTION("a unit name resolves")
  {
    struct pitch_port
    {
      static consteval auto name() { return "out"; }
      static consteval auto unit() { return "midipitch"; }
      int value;
    };
    CHECK(unit_of(port_for<pitch_port>()) == "time.midinote");
  }

  SECTION("a qualified unit name resolves")
  {
    struct mm_port
    {
      static consteval auto name() { return "out"; }
      static consteval auto unit() { return "distance.mm"; }
      float value;
    };
    CHECK(unit_of(port_for<mm_port>()) == "distance.mm");
  }

  SECTION("a dataspace name gives its neutral unit, not an empty dataspace")
  {
    struct pos_port
    {
      static consteval auto name() { return "out"; }
      static consteval auto unit() { return "position"; }
      float value;
    };
    auto p = port_for<pos_port>();
    auto u = p.type.target<ossia::unit_t>();
    REQUIRE(u);
    CHECK(bool(*u));
    CHECK(unit_of(p) == "position.cart3D");
  }

  SECTION("what a process declares wins over the shape of its value")
  {
    struct declared
    {
      static consteval auto name() { return "P"; }
      static consteval auto unit() { return "position.opengl"; }
      struct
      {
        float x, y, z;
      } value;
    };
    CHECK(unit_of(port_for<declared>()) == "position.openGL");
  }
}
