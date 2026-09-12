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

// A colour sent without its alpha.
//
// The gfx uniform update used ossia::convert<vec4f>(list), which starts from a
// zeroed vector and fills only what the list carries. Three floats therefore
// wrote a zero into the fourth component, and on a colour that is alpha: an
// OSC address carrying rgb turned the shape black. A cable never showed it,
// because it carries all four.
//
// This pins the rule the uniform update follows now: take what the list
// supplies, leave the rest alone.
namespace
{
template <std::size_t N>
void assign_prefix_ref(
    std::array<float, N>& val, const std::vector<ossia::value>& v) noexcept
{
  const std::size_t n = std::min(v.size(), N);
  for(std::size_t i = 0; i < n; i++)
    val[i] = ossia::convert<float>(v[i]);
}
}

TEST_CASE("a colour sent without alpha keeps the alpha it had", "[unit][color]")
{
  // What the old code did, for contrast: everything the list omits is zeroed.
  CHECK(ossia::convert<ossia::vec4f>(std::vector<ossia::value>{1.f, 1.f, 1.f})[3]
        == 0.f);

  ossia::vec4f uniform{0.2f, 0.4f, 0.6f, 1.f};
  assign_prefix_ref(uniform, std::vector<ossia::value>{1.f, 0.5f, 0.25f});

  CHECK(uniform[0] == 1.f);
  CHECK(uniform[1] == 0.5f);
  CHECK(uniform[2] == 0.25f);
  CHECK(uniform[3] == 1.f); // untouched, not zeroed

  SECTION("a full rgba still overwrites everything")
  {
    assign_prefix_ref(uniform, std::vector<ossia::value>{0.f, 0.f, 0.f, 0.5f});
    CHECK(uniform[3] == 0.5f);
  }
  SECTION("a longer list does not run past the uniform")
  {
    assign_prefix_ref(
        uniform, std::vector<ossia::value>{1.f, 1.f, 1.f, 1.f, 9.f, 9.f});
    CHECK(uniform[3] == 1.f);
  }
}

// The time chooser's second component is its mode: 0 means the value is in
// seconds, anything else means musical denominations. The port used to start
// at 1 while the object declaring it (halp::time_chooser_t) declares
// `sync{false}`, so a control's declared init was read as quarter notes and
// converted against the tempo -- two seconds became two quarters.
TEST_CASE("a time chooser starts in seconds, as its object declares", "[unit][ui]")
{
  Process::TimeChooser tc{0.f, 10.f, 2.f, "Duration", Id<Process::Port>{0}, nullptr};

  const auto v = ossia::convert<ossia::vec2f>(tc.value());
  CHECK(v[0] == 2.f);
  CHECK(v[1] == 0.f);

  // init mirrors it, so a reset does not flip the mode either.
  const auto i = ossia::convert<ossia::vec2f>(tc.init());
  CHECK(i[0] == 2.f);
  CHECK(i[1] == 0.f);
}
