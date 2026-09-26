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

#include <examples/Advanced/Utilities/Counter.hpp>

namespace
{
struct CounterRig
{
  examples::Counter c;
  int ceilings = 0;
  CounterRig(examples::Counter::Mode mode, int max, examples::Counter::OutputMode when)
  {
    c.inputs.ceil.value = mode;
    c.inputs.max.value = max;
    c.inputs.when.value = when;
    c.outputs.ceiling.call.context = this;
    c.outputs.ceiling.call.function
        = [](void* self) { static_cast<CounterRig*>(self)->ceilings++; };
  }
  //! One tick: n messages, optionally the Output and Reset bangs, then the
  //! processing. Returns what the Count outlet sends.
  std::optional<int> tick(int messages, bool output = false, bool reset = false)
  {
    for(int i = 0; i < messages; i++)
      c.increase();
    if(output)
      c.bang();
    if(reset)
      c.reset();
    c();
    return c.outputs.count.value;
  }
};
}

TEST_CASE("Counter: the Output bang sends the clipped count", "[avnd][utilities][counter]")
{
  CounterRig r{examples::Counter::Clip, 3, examples::Counter::Manually};
  CHECK_FALSE(r.tick(5));
  CHECK(r.tick(0, true) == 3);
  CounterRig w{examples::Counter::Wrap, 3, examples::Counter::Manually};
  CHECK(w.tick(4, true) == 1);
  CounterRig f{examples::Counter::Fold, 3, examples::Counter::Manually};
  CHECK(f.tick(4, true) == 2);
}

TEST_CASE("Counter: Reset starts over", "[avnd][utilities][counter]")
{
  CounterRig r{examples::Counter::Free, 100, examples::Counter::OnInput};
  CHECK(r.tick(3) == 3);
  CHECK(r.tick(0, false, true) == 0);
  CHECK(r.tick(1) == 1);
}

TEST_CASE("Counter: when the count is sent", "[avnd][utilities][counter]")
{
  SECTION("every tick, as before")
  {
    CounterRig r{examples::Counter::Free, 100, examples::Counter::EveryTick};
    CHECK(r.tick(2) == 2);
    CHECK(r.tick(0) == 2);
    CHECK(r.tick(0) == 2);
  }
  SECTION("on new input only")
  {
    CounterRig r{examples::Counter::Free, 100, examples::Counter::OnInput};
    CHECK(r.tick(2) == 2);
    CHECK_FALSE(r.tick(0));
    CHECK(r.tick(1) == 3);
  }
  SECTION("only on the Output bang")
  {
    CounterRig r{examples::Counter::Free, 100, examples::Counter::Manually};
    CHECK_FALSE(r.tick(2));
    CHECK_FALSE(r.tick(0, false, true)); // reset does not send either
    CHECK(r.tick(1, true) == 1);
    CHECK_FALSE(r.tick(0));
  }
  SECTION("Ceiling still fires on reaching Max")
  {
    CounterRig r{examples::Counter::Clip, 2, examples::Counter::Manually};
    r.tick(2);
    CHECK(r.ceilings == 1);
  }
}
