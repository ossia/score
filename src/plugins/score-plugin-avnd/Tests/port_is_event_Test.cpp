// A value inlet bound to an address either reads the address's value at every
// tick (is_event false) or only receives the messages the address gets
// (is_event true). A port whose value is optional -- it only has a value when
// something arrived -- is an event port, unless it says otherwise.

#include <ossia/dataflow/port.hpp>
#include <ossia/network/value/value.hpp>

#include <avnd/introspection/port.hpp>

#include <avnd/binding/ossia/port_setup.hpp>
#include <halp/controls.hpp>

#include <catch2/catch_all.hpp>

#include <examples/Advanced/Utilities/FlipFlop.hpp>

#include <optional>

namespace
{
template <typename Field>
bool is_event()
{
  ossia::value_port p;
  oscr::setup_value_port::setup_port<Field>(p);
  return p.is_event;
}

struct polled_optional : halp::val_port<"In", std::optional<float>>
{
  static constexpr bool event = false;
};
}

TEST_CASE("optional value ports are event ports", "[avnd][ports]")
{
  CHECK(is_event<halp::val_port<"In", std::optional<ossia::value>>>());
  CHECK(is_event<halp::val_port<"In", std::optional<float>>>());
  CHECK(is_event<halp::val_port<"In", std::optional<int>>>());
  CHECK(is_event<halp::val_port<"In", std::optional<std::string>>>());
  CHECK_FALSE(is_event<polled_optional>());
}

TEST_CASE("the flip flop only toggles on the messages it receives", "[avnd][ports]")
{
  using input_t = decltype(ao::FlipFlop{}.inputs.input);
  CHECK(is_event<input_t>());
}
