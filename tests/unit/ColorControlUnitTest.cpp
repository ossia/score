// A colour control driven from an address whose unit is color.rgb.
//
// Process::HSVSlider -- the colour control every shader and shape process
// carries -- declares unit() == rgba for the UI, but never told the execution
// port anything. value_port::effective_type() was therefore empty, and
// adapt_value() converts nothing when it has no sink type: an OSC parameter
// declared color.rgb wrote its three floats straight into a control that reads
// four, while color.rgba happened to line up and worked.

#include <Process/Dataflow/WidgetInlets.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/dataspace/color.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/value/value.hpp>

#include <catch2/catch_all.hpp>

#include <memory>

namespace
{
struct Fixture
{
  ossia::net::generic_device device{
      std::make_unique<ossia::net::multiplex_protocol>(), "test"};

  //! A device parameter carrying a colour in the given unit, the way an OSC
  //! device declares one.
  ossia::net::parameter_base& color_param(const char* name, ossia::unit_t u)
  {
    auto& node = ossia::net::create_node(device, name);
    auto* p = node.create_parameter(ossia::underlying_type(u));
    p->set_unit(u);
    return *p;
  }
};
}

TEST_CASE("a colour control declares its unit to the execution port")
{
  Process::HSVSlider inlet{
      ossia::vec4f{0.f, 0.f, 0.f, 1.f}, "Color", Id<Process::Port>{0}, nullptr};

  ossia::value_inlet exec;
  inlet.setupExecution(exec, nullptr);

  const auto* u = (*exec).effective_unit();
  REQUIRE(u != nullptr);
  CHECK(*u == ossia::unit_t{ossia::rgba_u{}});
}

TEST_CASE("a color.rgb address reaches a colour control as rgba")
{
  Fixture f;
  auto& param = f.color_param("rgb", ossia::unit_t{ossia::rgb_u{}});

  Process::HSVSlider inlet{
      ossia::vec4f{0.f, 0.f, 0.f, 1.f}, "Color", Id<Process::Port>{0}, nullptr};
  ossia::value_inlet exec;
  inlet.setupExecution(exec, nullptr);

  (*exec).add_global_value(param, ossia::make_vec(1.f, 0.5f, 0.25f));

  const auto& data = (*exec).get_data();
  REQUIRE(data.size() == 1);
  REQUIRE(data[0].value.get_type() == ossia::val_type::VEC4F);
  const auto v = data[0].value.get<ossia::vec4f>();
  CHECK(v[0] == Catch::Approx(1.f));
  CHECK(v[1] == Catch::Approx(0.5f));
  CHECK(v[2] == Catch::Approx(0.25f));
  CHECK(v[3] == Catch::Approx(1.f)); // opaque, the neutral alpha rgb implies
}

// The case that already worked, kept so the two stay in step.
TEST_CASE("a color.rgba address reaches a colour control unchanged")
{
  Fixture f;
  auto& param = f.color_param("rgba", ossia::unit_t{ossia::rgba_u{}});

  Process::HSVSlider inlet{
      ossia::vec4f{0.f, 0.f, 0.f, 1.f}, "Color", Id<Process::Port>{0}, nullptr};
  ossia::value_inlet exec;
  inlet.setupExecution(exec, nullptr);

  (*exec).add_global_value(param, ossia::make_vec(1.f, 0.5f, 0.25f, 0.75f));

  const auto& data = (*exec).get_data();
  REQUIRE(data.size() == 1);
  REQUIRE(data[0].value.get_type() == ossia::val_type::VEC4F);
  const auto v = data[0].value.get<ossia::vec4f>();
  CHECK(v[0] == Catch::Approx(1.f));
  CHECK(v[3] == Catch::Approx(0.75f));
}

// An address in another colour unit entirely has to arrive converted too.
TEST_CASE("a color.hsv address reaches a colour control as rgba")
{
  Fixture f;
  auto& param = f.color_param("hsv", ossia::unit_t{ossia::hsv_u{}});

  Process::HSVSlider inlet{
      ossia::vec4f{0.f, 0.f, 0.f, 1.f}, "Color", Id<Process::Port>{0}, nullptr};
  ossia::value_inlet exec;
  inlet.setupExecution(exec, nullptr);

  // Pure red in HSV.
  (*exec).add_global_value(param, ossia::make_vec(0.f, 1.f, 1.f));

  const auto& data = (*exec).get_data();
  REQUIRE(data.size() == 1);
  REQUIRE(data[0].value.get_type() == ossia::val_type::VEC4F);
  const auto v = data[0].value.get<ossia::vec4f>();
  CHECK(v[0] == Catch::Approx(1.f).margin(1e-4));
  CHECK(v[1] == Catch::Approx(0.f).margin(1e-4));
  CHECK(v[2] == Catch::Approx(0.f).margin(1e-4));
}
