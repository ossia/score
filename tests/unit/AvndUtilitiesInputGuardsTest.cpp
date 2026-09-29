// Value delay and Enumerator given out-of-range values through a cable: the
// controls' ranges only bound what the inspector sends.

#include <Advanced/Utilities/Enumerator.hpp>
#include <Helpers/ValueDelay.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <vector>

TEST_CASE("Value delay: a huge tap count is bounded", "[avnd][utilities][delay]")
{
  using VD = examples::helpers::ValueDelay;
  VD d;
  d.inputs.mode.value = VD::Ticks;
  d.inputs.length.value = 1;
  d.inputs.count.value = std::numeric_limits<int>::max();
  d.prepare(halp::setup{.rate = 1000.});

  d.inputs.in.value = ossia::value{1.f};
  d(halp::tick{.frames = 1});
  CHECK(d.outputs.a.value.size() == VD::max_taps);
}

TEST_CASE("Enumerator: a non-finite or huge index is no index", "[avnd][utilities][enumerator]")
{
  using E = ao::Enumerator;
  E e;
  e.inputs.mode.value = E::Manual;
  e.inputs.bounds.value = E::Wrap;
  e.prepare(halp::setup{.rate = 1000.});
  e.inputs.list.value = ossia::value{std::vector<ossia::value>{10, 20, 30}};

  auto tick = [&](ossia::value trig) {
    e.inputs.trigger.value = std::move(trig);
    e(halp::tick{.frames = 1});
    return e.outputs.value.value;
  };

  CHECK_FALSE(tick(std::numeric_limits<float>::quiet_NaN()));
  CHECK_FALSE(tick(std::numeric_limits<float>::infinity()));

  // Out of the range of the rounding: bounded, then wrapped into the list
  const auto v = tick(std::numeric_limits<float>::max());
  REQUIRE(v);
  CHECK((*v == ossia::value{10} || *v == ossia::value{20} || *v == ossia::value{30}));
  CHECK(tick(-std::numeric_limits<float>::max()));
}
