// An enum control published as a score:/controls parameter is a string
// parameter: what a state, a script or OSC sends reaches the inlet through it.

#include <ossia/dataflow/port.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <avnd/introspection/port.hpp>

#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/port_setup.hpp>
#include <halp/controls.hpp>

#include <catch2/catch_all.hpp>

namespace
{
enum class Mode
{
  Play,
  Record,
  Overdub,
  Stop
};
struct mode_control : halp::enum_t<Mode, "Loop">
{
  using halp::enum_t<Mode, "Loop">::operator=;
};

Mode received(const ossia::value& sent)
{
  ossia::net::generic_device dev{"controls"};
  auto& node = ossia::net::create_node(dev.get_root_node(), "/loop");
  auto& param = *node.create_parameter(ossia::val_type::STRING);

  ossia::value_port port;
  oscr::setup_value_port::setup_port<mode_control>(port);
  port.add_global_values(param, {sent});
  REQUIRE(port.get_data().size() == 1);

  mode_control field;
  field.value = Mode::Stop;
  oscr::from_ossia_value(field, port.get_data()[0].value, field.value);
  return field.value;
}
}

TEST_CASE("An enum control follows its string parameter", "[avnd][port][enum]")
{
  CHECK(received(std::string{"Record"}) == Mode::Record);
  CHECK(received(std::string{"Overdub"}) == Mode::Overdub);
  CHECK(received(std::string{"1"}) == Mode::Record);
  CHECK(received(2) == Mode::Overdub);
}
