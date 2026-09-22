// A modulation source drives a control whose range is much wider than its own:
// the value has to arrive rescaled, and widened to the arity the sink expects.

#include <ossia/dataflow/port.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <avnd/introspection/port.hpp>

#include <avnd/binding/ossia/port_setup.hpp>
#include <halp/controls.hpp>

#include <catch2/catch_all.hpp>

#include <optional>

namespace
{
// As the LFO declares its output.
struct lfo_out : halp::val_port<"Out", std::optional<float>>
{
  struct range
  {
    float min = 0.;
    float max = 1.;
  };
};

// As every Threedim primitive declares its rotation.
using rotation = halp::xyz_spinboxes_f32<"Rotation", halp::range{0., 359.9999999, 0.}>;

template <typename Field>
ossia::value_port port_for()
{
  ossia::value_port p;
  oscr::setup_value_port::setup_port<Field>(p);
  return p;
}

ossia::value drive(ossia::value_port& src, ossia::value_port& sink, ossia::value v)
{
  src.write_value(std::move(v), 0);
  sink.add_port_values(src);
  REQUIRE(sink.get_data().size() == 1);
  return sink.get_data()[0].value;
}

ossia::vec3f vec3_of(const ossia::value& v)
{
  const auto* p = v.target<ossia::vec3f>();
  REQUIRE(p);
  return *p;
}
}

TEST_CASE("An LFO sweeps a rotation control's whole range", "[avnd][port][domain]")
{
  auto src = port_for<lfo_out>();
  auto sink = port_for<rotation>();

  SECTION("both ends declare their range")
  {
    auto [smin, smax] = ossia::get_float_minmax(src.domain);
    REQUIRE(smin);
    REQUIRE(smax);
    CHECK(*smin == Catch::Approx(0.));
    CHECK(*smax == Catch::Approx(1.));

    auto [dmin, dmax] = ossia::get_float_minmax(sink.domain);
    REQUIRE(dmin);
    REQUIRE(dmax);
    CHECK(*dmin == Catch::Approx(0.));
    CHECK(*dmax == Catch::Approx(360.).margin(0.001));
  }

  SECTION("the top of the source range reaches the top of the sink's")
  {
    const auto v = vec3_of(drive(src, sink, 1.0f));
    CHECK(v[0] == Catch::Approx(360.).margin(0.001));
    CHECK(v[1] == Catch::Approx(360.).margin(0.001));
    CHECK(v[2] == Catch::Approx(360.).margin(0.001));
  }

  SECTION("the middle of the source range reaches the middle of the sink's")
  {
    const auto v = vec3_of(drive(src, sink, 0.5f));
    CHECK(v[0] == Catch::Approx(180.).margin(0.001));
    CHECK(v[1] == Catch::Approx(180.).margin(0.001));
    CHECK(v[2] == Catch::Approx(180.).margin(0.001));
  }
}
