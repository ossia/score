// The control utilities of avendish's examples, driven tick by tick as the
// score executor does: inputs set, operator() called, outputs read.
#include <ossia/network/value/value.hpp>

#include <catch2/catch_all.hpp>

#include <examples/Advanced/Utilities/Spigot.hpp>

TEST_CASE("Spigot lets messages through only while enabled", "[avnd][utilities]")
{
  ao::Spigot s;
  auto tick = [&](std::optional<ossia::value> in, bool enabled) {
    s.inputs.input.value = std::move(in);
    s.inputs.enabled.value = enabled;
    s();
    return s.outputs.output.value;
  };
  CHECK(tick(ossia::value{1.f}, true) == ossia::value{1.f});
  // Nothing new: nothing out, the previous value is not sent again.
  CHECK_FALSE(tick(std::nullopt, true));
  CHECK_FALSE(tick(ossia::value{2.f}, false));
  CHECK(tick(ossia::value{3.f}, true) == ossia::value{3.f});
}
