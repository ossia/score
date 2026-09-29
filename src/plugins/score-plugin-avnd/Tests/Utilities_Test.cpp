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

#include <AvndProcesses/Queue.hpp>

namespace
{
struct QueueRig
{
  avnd_tools::Queue q;
  explicit QueueRig(avnd_tools::Queue::OutputMode mode, int length = 3)
  {
    q.inputs.mode.value = mode;
    q.inputs.length.value = length;
    q.inputs.length.update(q);
  }
  std::optional<ossia::value>
  tick(std::optional<ossia::value> in, bool bang = false, bool clear = false)
  {
    q.inputs.input.value = std::move(in);
    q.inputs.clear.value = clear;
    if(bang)
      q.inputs.bang.update(q);
    q();
    return q.outputs.output.value;
  }
};
}

TEST_CASE("Buffer queue: held Clear empties and blocks the queue", "[avnd][utilities][queue]")
{
  QueueRig r{avnd_tools::Queue::OnBang};
  r.q.inputs.data.value = avnd_tools::Queue::WholeBuffer;
  r.tick(ossia::value{1});
  r.tick(ossia::value{2});
  r.tick(ossia::value{3}, false, true); // held
  r.tick(ossia::value{4}, false, true);
  CHECK(r.q.buffer.empty()); // cleared, and nothing queued while held
  r.tick(ossia::value{5});
  auto out = r.tick(std::nullopt, true);
  REQUIRE(out);
  CHECK(out->get<std::vector<ossia::value>>() == std::vector<ossia::value>{5});
}

TEST_CASE("Buffer queue: Bang sends in any mode, like Counter's Output", "[avnd][utilities][queue]")
{
  SECTION("when full, before it is")
  {
    QueueRig r{avnd_tools::Queue::WhenFull};
    CHECK_FALSE(r.tick(ossia::value{1}));
    CHECK(r.tick(std::nullopt, true) == ossia::value{1});
  }
  SECTION("on bang: once per bang, not again at every tick")
  {
    QueueRig r{avnd_tools::Queue::OnBang};
    CHECK_FALSE(r.tick(ossia::value{1}));
    CHECK(r.tick(std::nullopt, true) == ossia::value{1});
    CHECK_FALSE(r.tick(std::nullopt));
  }
  SECTION("on bang, pop oldest")
  {
    QueueRig r{avnd_tools::Queue::OnBangPopOldest};
    r.tick(ossia::value{1});
    r.tick(ossia::value{2});
    CHECK(r.tick(std::nullopt, true) == ossia::value{1});
    CHECK(r.tick(std::nullopt, true) == ossia::value{2});
  }
  SECTION("an input is queued once, not at every tick after it")
  {
    QueueRig r{avnd_tools::Queue::OnBangPopOldest, 10};
    r.tick(ossia::value{1});
    r.tick(std::nullopt);
    r.tick(std::nullopt);
    CHECK(r.q.buffer.size() == 1);
  }
}

#include <examples/Helpers/ValueDelay.hpp>
#include <ossia/network/value/value_conversion.hpp>

namespace
{
struct DelayRig
{
  using VD = examples::helpers::ValueDelay;
  VD d;
  DelayRig(VD::Mode mode, int length, int count)
  {
    d.inputs.mode.value = mode;
    d.inputs.length.value = length;
    d.inputs.count.value = count;
    d.prepare(halp::setup{.rate = 1000.});
  }
  // One tick of `ms` milliseconds (at 1 kHz, one frame per ms), with an
  // optional new value on In. The binding empties In after each tick.
  std::vector<ossia::value> tick(std::optional<ossia::value> in, int ms = 1)
  {
    d.inputs.in.value = std::move(in);
    d(halp::tick{.frames = ms});
    d.inputs.in.value.reset();
    return d.outputs.a.value;
  }
  float f(const ossia::value& v) { return ossia::convert<float>(v); }
};
using VD = examples::helpers::ValueDelay;
}

TEST_CASE("Value delay: in messages", "[avnd][utilities][delay]")
{
  // Two taps, two messages apart
  DelayRig r{VD::Messages, 2, 2};
  for(float v : {1.f, 2.f, 3.f, 4.f})
    r.tick(v);
  // Ticks without messages do not move it
  r.tick(std::nullopt, 50);
  auto out = r.tick(5.f);
  REQUIRE(out.size() == 2);
  CHECK(out[0] == ossia::value{3.f}); // 2 messages ago
  CHECK(out[1] == ossia::value{1.f}); // 4 messages ago
}

TEST_CASE("Value delay: in time", "[avnd][utilities][delay]")
{
  // Two taps, 10 ms apart; Length does not matter in this mode
  DelayRig r{VD::Time, 1000, 2};
  r.d.inputs.time.value = 0.01f;
  r.tick(1.f, 5);         // t = 0: 1
  r.tick(2.f, 10);        // t = 5: 2
  r.tick(3.f, 10);        // t = 15: 3
  auto out = r.tick(std::nullopt, 1); // t = 25
  REQUIRE(out.size() == 2);
  CHECK(out[0] == ossia::value{3.f}); // at t = 15
  CHECK(out[1] == ossia::value{2.f}); // at t = 5
  // It follows real time, not the number of ticks
  out = r.tick(std::nullopt, 1); // t = 26
  CHECK(out[1] == ossia::value{2.f});
}

TEST_CASE("Value delay: in ticks, Length ticks apart", "[avnd][utilities][delay]")
{
  DelayRig r{VD::Ticks, 3, 2};
  for(int i = 1; i <= 10; i++)
    r.tick(float(i));
  // Tick 10 holds 10: tap 0 is 3 ticks ago, tap 1 6 ticks ago
  auto out = r.tick(11.f);
  REQUIRE(out.size() == 2);
  CHECK(out[0] == ossia::value{8.f});
  CHECK(out[1] == ossia::value{5.f});
}

TEST_CASE("Value delay: any value", "[avnd][utilities][delay]")
{
  DelayRig r{VD::Messages, 1, 2};
  r.tick(std::string{"a"});
  r.tick(ossia::vec3f{1.f, 2.f, 3.f});
  auto out = r.tick(std::vector<ossia::value>{1, std::string{"x"}});
  REQUIRE(out.size() == 2);
  CHECK(out[0] == ossia::value{ossia::vec3f{1.f, 2.f, 3.f}});
  CHECK(out[1] == ossia::value{std::string{"a"}});
}

TEST_CASE("Value delay: feedback repeats a movement and fades it", "[avnd][utilities][delay]")
{
  // One message apart, feedback 0.5: a 1 among 0s comes back halved each
  // message; a steady value stays what it is.
  DelayRig r{VD::Messages, 1, 3};
  r.d.inputs.feedback.value = 0.5f;
  r.tick(0.f);
  r.tick(1.f);
  auto out = r.tick(0.f);
  CHECK(r.f(out[0]) == Catch::Approx(0.5f)); // what was recorded for 1: (1-0.5)*1 + 0.5*0
  out = r.tick(0.f);
  CHECK(r.f(out[0]) == Catch::Approx(0.25f)); // its echo
  CHECK(r.f(out[1]) == Catch::Approx(0.5f));

  DelayRig steady{VD::Messages, 1, 2};
  steady.d.inputs.feedback.value = 0.9f;
  for(int i = 0; i < 20; i++)
    out = steady.tick(2.f);
  CHECK(steady.f(out[0]) == Catch::Approx(2.f));
  CHECK(steady.f(out[1]) == Catch::Approx(2.f));
}

TEST_CASE("Value delay: freeze loops, clear empties", "[avnd][utilities][delay]")
{
  // Ticks, 2 apart: 1 2 3 4, then frozen: the line repeats its last 2 ticks.
  DelayRig r{VD::Ticks, 2, 1};
  for(float v : {1.f, 2.f, 3.f, 4.f})
    r.tick(v);
  r.d.inputs.freeze.value = true;
  std::vector<float> seen;
  for(float v : {10.f, 11.f, 12.f, 13.f})
    seen.push_back(r.f(r.tick(v)[0]));
  CHECK(seen == std::vector<float>{3.f, 4.f, 3.f, 4.f});

  // Clear: the line is gone, the taps read what In is now.
  r.d.inputs.freeze.value = false;
  r.d.inputs.clear.update(r.d);
  auto out = r.tick(7.f);
  CHECK(r.f(out[0]) == 7.f);
}

TEST_CASE("Value delay: smooth glides between the recorded values", "[avnd][utilities][delay]")
{
  DelayRig r{VD::Time, 1, 1};
  r.d.inputs.time.value = 0.01f; // 10 ms
  r.d.inputs.smooth.value = true;
  r.tick(0.f, 10);  // t = 0: 0
  r.tick(10.f, 10); // t = 10: 10
  auto out = r.tick(std::nullopt, 5); // t = 20 -> reads t = 10: 10
  CHECK(r.f(out[0]) == Catch::Approx(10.f));

  DelayRig step{VD::Time, 1, 1};
  step.d.inputs.time.value = 0.01f;
  step.tick(0.f, 10);
  step.tick(10.f, 5); // t = 10: 10
  out = step.tick(std::nullopt, 1); // t = 15 -> reads t = 5: 0, stepped
  CHECK(step.f(out[0]) == 0.f);

  r = DelayRig{VD::Time, 1, 1};
  r.d.inputs.time.value = 0.01f;
  r.d.inputs.smooth.value = true;
  r.tick(0.f, 10);
  r.tick(10.f, 5);
  out = r.tick(std::nullopt, 1); // reads t = 5: halfway
  CHECK(r.f(out[0]) == Catch::Approx(5.f));
}

TEST_CASE("Value delay: the Mix outlet", "[avnd][utilities][delay]")
{
  DelayRig r{VD::Messages, 1, 1};
  r.d.inputs.mix.value = 0.25f;
  r.tick(0.f);
  r.tick(8.f);
  // In is 8, its first echo 0
  CHECK(r.f(r.d.outputs.mix.value) == Catch::Approx(6.f));
  // Values that do not mix: In below 0.5, the echo from 0.5
  r.tick(std::string{"b"});
  CHECK(r.d.outputs.mix.value == ossia::value{std::string{"b"}});
  r.d.inputs.mix.value = 1.f;
  r.tick(std::string{"c"});
  CHECK(r.d.outputs.mix.value == ossia::value{std::string{"b"}});
}

#include <examples/Advanced/Utilities/ArrayRecombiner.hpp>

TEST_CASE("Array recombiner groups vec2f / vec3f / vec4f too", "[avnd][utilities][array]")
{
  ao::ArrayRecombiner r;
  r.inputs.elements.value = 2;
  using list = std::vector<ossia::value>;
  CHECK(r(ossia::vec4f{1.f, 2.f, 3.f, 4.f}) == list{list{1.f, 2.f}, list{3.f, 4.f}});
  CHECK(r(ossia::vec3f{1.f, 2.f, 3.f}) == list{list{1.f, 2.f}, list{3.f}});
  CHECK(r(ossia::vec2f{1.f, 2.f}) == list{list{1.f, 2.f}});
  r.inputs.transpose.value = true;
  CHECK(r(ossia::vec4f{1.f, 2.f, 3.f, 4.f}) == list{list{1.f, 3.f}, list{2.f, 4.f}});
  // Lists as before
  r.inputs.transpose.value = false;
  CHECK(r(list{1, 2, 3}) == list{list{1, 2}, list{3}});
}

#include <examples/Advanced/Image/LightnessSampler.hpp>

TEST_CASE("Lightness sampler draws the image at its aspect ratio", "[avnd][utilities][image]")
{
  using D = vo::LightnessSamplerTextureDisplay;
  // 16:9 in the 200 x 200 item: full width, letterboxed vertically
  auto r = D::imageRect(1920, 1080);
  CHECK(r.w == 200.);
  CHECK(r.h == Catch::Approx(112.5));
  CHECK(r.x == 0.);
  CHECK(r.y == Catch::Approx((200. - 112.5) / 2.));
  // Portrait: full height, pillarboxed
  r = D::imageRect(100, 400);
  CHECK(r.h == 200.);
  CHECK(r.w == 50.);
  CHECK(r.x == 75.);
  // Square: the whole item
  r = D::imageRect(64, 64);
  CHECK(r.w == 200.);
  CHECK(r.h == 200.);
}

#include <examples/Advanced/Utilities/Enumerator.hpp>

namespace
{
struct EnumRig
{
  ao::Enumerator e;
  explicit EnumRig(ao::Enumerator::Mode mode, ao::Enumerator::Bounds bounds)
  {
    e.inputs.mode.value = mode;
    e.inputs.bounds.value = bounds;
    e.inputs.list.value = std::vector<ossia::value>{10, 20, 30};
    e.prepare(halp::setup{.rate = 1000.});
  }
  std::optional<ossia::value> tick(std::optional<ossia::value> trig = {}, int frames = 1)
  {
    e.inputs.trigger.value = std::move(trig);
    e(halp::tick{frames});
    return e.outputs.value.value;
  }
};
const ossia::value bang{ossia::impulse{}};
}

TEST_CASE("Enumerator: bangs walk the list with each bound mode", "[avnd][utilities][enumerator]")
{
  using E = ao::Enumerator;
  auto walk = [](E::Bounds b) {
    EnumRig r{E::Manual, b};
    std::vector<int> seen;
    for(int i = 0; i < 6; i++)
      seen.push_back(r.tick(bang)->get<int>());
    return seen;
  };
  CHECK(walk(E::Clip) == std::vector<int>{10, 20, 30, 30, 30, 30});
  CHECK(walk(E::Wrap) == std::vector<int>{10, 20, 30, 10, 20, 30});
  CHECK(walk(E::Fold) == std::vector<int>{10, 20, 30, 20, 10, 20});
}

TEST_CASE("Enumerator: a number is an index, nothing without a trigger", "[avnd][utilities][enumerator]")
{
  EnumRig r{ao::Enumerator::Manual, ao::Enumerator::Wrap};
  CHECK_FALSE(r.tick());
  CHECK(r.tick(ossia::value{2}) == ossia::value{30});
  CHECK(r.e.outputs.index.value == 2);
  CHECK(r.tick(ossia::value{4}) == ossia::value{20}); // wrapped
  CHECK(r.tick(bang) == ossia::value{30});             // continues from there
  // Same index again: the value goes out, the index does not
  r.tick(ossia::value{2});
  CHECK(r.e.outputs.value.value == ossia::value{30});
  CHECK_FALSE(r.e.outputs.index.value);
}

TEST_CASE("Enumerator: automatic modes", "[avnd][utilities][enumerator]")
{
  SECTION("every tick")
  {
    EnumRig r{ao::Enumerator::EveryTick, ao::Enumerator::Wrap};
    CHECK(r.tick() == ossia::value{10});
    CHECK(r.tick() == ossia::value{20});
  }
  SECTION("at an interval")
  {
    EnumRig r{ao::Enumerator::Timed, ao::Enumerator::Wrap};
    r.e.inputs.interval.value = 0.1f; // 100 ms, at 1 kHz = 100 frames
    int sent = 0;
    for(int i = 0; i < 10; i++) // 10 ticks of 50 ms
      sent += bool(r.tick({}, 50));
    CHECK(sent == 5);
  }
  SECTION("vec3f lists")
  {
    EnumRig r{ao::Enumerator::Manual, ao::Enumerator::Clip};
    r.e.inputs.list.value = ossia::vec3f{1.f, 2.f, 3.f};
    r.tick(bang);
    CHECK(r.tick(bang) == ossia::value{2.f});
  }
}
