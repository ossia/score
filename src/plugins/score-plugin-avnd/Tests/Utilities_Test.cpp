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

#include <examples/Advanced/Utilities/Accumulator.hpp>

namespace
{
std::optional<float> tick(ao::Accumulator& a, std::optional<float> in, bool output = false)
{
  a.inputs.in.value = in;
  if(output)
    a.inputs.output.update(a);
  a();
  return a.outputs.sum.value;
}
}

TEST_CASE("Accumulator: when the statistics are sent", "[avnd][utilities][accumulator]")
{
  SECTION("every tick, as before")
  {
    ao::Accumulator a;
    CHECK(tick(a, 1.f) == 1.f);
    CHECK(tick(a, std::nullopt) == 1.f);
  }
  SECTION("on new input only")
  {
    ao::Accumulator a;
    a.inputs.when.value = ao::Accumulator::OnInput;
    CHECK(tick(a, 1.f) == 1.f);
    CHECK_FALSE(tick(a, std::nullopt));
    CHECK(tick(a, 2.f) == 3.f);
  }
  SECTION("only on the Output bang")
  {
    ao::Accumulator a;
    a.inputs.when.value = ao::Accumulator::Manually;
    CHECK_FALSE(tick(a, 1.f));
    CHECK_FALSE(tick(a, 2.f));
    CHECK(tick(a, std::nullopt, true) == 3.f);
    CHECK(a.outputs.count.value == 2.f);
    CHECK_FALSE(tick(a, std::nullopt));
  }
}

TEST_CASE("Accumulator: reset forgets everything", "[avnd][utilities][accumulator]")
{
  ao::Accumulator a;
  a.inputs.when.value = ao::Accumulator::OnInput;
  tick(a, 1.f);
  tick(a, 5.f);
  a.inputs.reset.update(a);
  // Reset is sent once, as zeros.
  CHECK(tick(a, std::nullopt) == 0.f);
  CHECK(a.outputs.count.value == 0.f);
  CHECK_FALSE(tick(a, std::nullopt));
  // The consecutive difference starts over too.
  tick(a, 2.f);
  CHECK(a.outputs.diff.value == 2.f);
}
