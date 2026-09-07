#include <Fx/RateLimiter.hpp>

#include <ossia/dataflow/execution_state.hpp>

#include <avnd/binding/ossia/data_node.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

namespace
{
using Node = Nodes::RateLimiter::Node;
using Mode = Node::Mode;
using Event = std::pair<int64_t, ossia::value>;
constexpr int64_t ms = Node::flicks_per_ms;

struct Limiter
{
  ossia::execution_state state;
  ossia::value_port input;
  Node node;
  std::vector<Event> output;

  Limiter()
  {
    state.bufferSize = 4096;
    state.modelToSamplesRatio = 48000. / 705600000.;
    state.samplesToModelRatio = 1. / state.modelToSamplesRatio;
    node.ossia_state = {&state};
    node.inputs.port.value = &input;
    node.outputs.out.call.context = this;
    node.outputs.out.call.function
        = [](void* context, int64_t frame, ossia::value value) {
      static_cast<Limiter*>(context)->output.emplace_back(frame, std::move(value));
    };
  }

  void debounce(int delay)
  {
    node.inputs.mode.value = Mode::Debounce;
    node.inputs.mode.update(node);
    node.inputs.ms.value = delay;
  }

  void add(int64_t frame, ossia::value value)
  {
    input.write_value(std::move(value), frame);
  }

  static Node::tick tick(int64_t begin, int64_t end, int frames, int offset = 0)
  {
    Node::tick t;
    t.prev_date = ossia::time_value{begin};
    t.date = ossia::time_value{end};
    t.speed = end >= begin ? 1. : -1.;
    t.start_sample = offset;
    t.length_sample = frames;
    t.signature = {4, 4};
    t.musical_start_position = begin / 352800000.;
    t.musical_end_position = end / 352800000.;
    return t;
  }

  void run(Node::tick t)
  {
    node(t);
    input.clear();
  }

  void run(int64_t begin, int64_t end, int frames = 480, int offset = 0)
  {
    run(tick(begin, end, frames, offset));
  }
};
}

TEST_CASE(
    "Rate limiter debounce owns deadlines at the next tick boundary", "[rate-limiter]")
{
  Limiter f;
  f.debounce(10);
  f.add(0, "first");
  f.run(0, 10 * ms);
  REQUIRE(f.output.empty());
  // Re-sending the current mode is not a transition and must not cancel it.
  f.node.inputs.mode.update(f.node);
  f.run(10 * ms, 20 * ms);
  REQUIRE(f.output == std::vector<Event>{{0, "first"}});
  f.run(20 * ms, 30 * ms);
  REQUIRE(f.output == std::vector<Event>{{0, "first"}});
}

TEST_CASE(
    "Rate limiter debounce retains the latest arbitrary value across a burst",
    "[rate-limiter]")
{
  Limiter f;
  f.debounce(10);
  const ossia::value latest = ossia::value_map_type{
      {"bytes", std::string{"a\0b", 3}},
      {"nested", std::vector<ossia::value>{true, ossia::impulse{}, 3.5f}}};
  f.add(384, 1); // 8 ms
  f.run(0, 10 * ms);
  f.add(192, 2);      // 14 ms
  f.add(432, latest); // 19 ms
  f.run(10 * ms, 20 * ms);
  REQUIRE(f.output.empty());
  f.run(20 * ms, 30 * ms);
  REQUIRE(f.output == std::vector<Event>{{432, latest}});
}

TEST_CASE(
    "Rate limiter debounce processes several quiet intervals within a tick",
    "[rate-limiter]")
{
  Limiter f;
  f.debounce(2);
  f.add(0, 1);
  f.add(144, 2); // 3 ms
  f.add(336, 3); // 7 ms
  f.run(0, 10 * ms);
  REQUIRE(f.output == std::vector<Event>{{96, 1}, {240, 2}, {432, 3}});
}

TEST_CASE(
    "Rate limiter debounce orders fan-in timestamps without losing equal-time order",
    "[rate-limiter]")
{
  Limiter f;
  f.debounce(2);
  f.add(336, 3);
  f.add(0, 1);
  f.add(144, 2);
  f.add(144, "last at 3 ms");
  f.run(0, 10 * ms);
  REQUIRE(f.output == std::vector<Event>{{96, 1}, {240, "last at 3 ms"}, {432, 3}});
}

TEST_CASE(
    "Rate limiter debounce equal values reset the quiet interval", "[rate-limiter]")
{
  Limiter f;
  f.debounce(10);
  f.add(0, 42);
  f.run(0, 10 * ms);
  f.add(0, 42); // Exactly at the old deadline: the new arrival wins.
  f.add(240, 42);
  f.run(10 * ms, 20 * ms);
  REQUIRE(f.output.empty());
  f.run(20 * ms, 30 * ms);
  REQUIRE(f.output == std::vector<Event>{{240, 42}});
}

TEST_CASE("Rate limiter zero debounce emits every event immediately", "[rate-limiter]")
{
  Limiter f;
  f.debounce(0);
  f.add(5, 42);
  f.add(5, 42);
  f.add(17, "next");
  f.run(0, 10 * ms);
  REQUIRE(f.output == std::vector<Event>{{5, 42}, {5, 42}, {17, "next"}});
}

TEST_CASE(
    "Rate limiter debounce ignores quantization and uses model time at transport speed",
    "[rate-limiter]")
{
  Limiter f;
  f.debounce(4);
  f.node.inputs.quantification.value = 1.;
  f.add(48, "value"); // 2 model ms in a tick running at twice nominal speed.
  auto t = Limiter::tick(10 * ms, 30 * ms, 480);
  t.speed = 2.;
  f.run(t);
  REQUIRE(f.output == std::vector<Event>{{144, "value"}});
}

TEST_CASE(
    "Rate limiter debounce rounds fractional deadlines up and never emits outside a "
    "slice",
    "[rate-limiter]")
{
  Limiter f;
  f.debounce(1);
  f.add(4, "outside before");
  f.add(9, "inside"); // Slice frame 4, at 0.8 ms.
  f.add(15, "outside after");
  f.run(0, 2 * ms, 10, 5);
  REQUIRE(f.output == std::vector<Event>{{9, "inside"}});

  f.output.clear();
  f.add(3, "deferred"); // 1.8 ms + 1 ms: first eligible sample is the next tick.
  f.run(2 * ms, 5 * ms, 5);
  REQUIRE(f.output.empty());
  f.run(5 * ms, 8 * ms, 5);
  REQUIRE(f.output == std::vector<Event>{{0, "deferred"}});
}

TEST_CASE(
    "Rate limiter debounce rounds a sub-sample deadline to the first eligible sample",
    "[rate-limiter]")
{
  Limiter f;
  f.debounce(1);
  f.add(0, "fractional");
  f.run(0, 10 * ms, 441);
  REQUIRE(f.output == std::vector<Event>{{45, "fractional"}});
}

TEST_CASE("Rate limiter mode transitions cancel pending values", "[rate-limiter]")
{
  Limiter f;
  f.debounce(20);
  f.add(0, "stale");
  f.run(0, 10 * ms);

  SECTION("UI updates cancel even when switching back before the next tick")
  {
    f.node.inputs.mode.value = Mode::Limit;
    f.node.inputs.mode.update(f.node);
    f.node.inputs.mode.value = Mode::Debounce;
    f.node.inputs.mode.update(f.node);
  }
  SECTION("direct control changes are detected at execution")
  {
    f.node.inputs.mode.value = Mode::Limit;
    f.run(10 * ms, 20 * ms);
    f.node.inputs.mode.value = Mode::Debounce;
  }
  f.run(20 * ms, 30 * ms);
  REQUIRE(f.output.empty());
  f.add(0, "fresh");
  f.run(30 * ms, 40 * ms);
  f.run(40 * ms, 50 * ms);
  f.run(50 * ms, 60 * ms);
  REQUIRE(f.output == std::vector<Event>{{0, "fresh"}});
}

TEST_CASE(
    "Rate limiter lifecycle and transport discontinuities discard pending debounce",
    "[rate-limiter]")
{
  Limiter f;
  f.debounce(20);
  f.add(0, "stale");
  f.run(100 * ms, 110 * ms);

  SECTION("stop and restart at the same date")
  {
    f.node.stop();
    f.node.start();
  }
  SECTION("explicit reset")
  {
    f.node.reset();
  }
  SECTION("prepare")
  {
    f.node.prepare({});
  }
  SECTION("transport callback")
  {
    f.node.transport(110 * ms);
  }
  SECTION("rewind tick")
  {
    f.add(0, "reverse input");
    f.run(110 * ms, 100 * ms);
    f.run(100 * ms, 110 * ms);
  }
  SECTION("backward seek")
  {
    f.run(90 * ms, 110 * ms);
  }
  SECTION("forward seek")
  {
    f.run(130 * ms, 140 * ms);
  }
  SECTION("stopped clock")
  {
    f.run(110 * ms, 110 * ms);
  }

  f.run(110 * ms, 130 * ms);
  REQUIRE(f.output.empty());
  f.add(0, "fresh");
  f.run(130 * ms, 140 * ms);
  f.run(140 * ms, 150 * ms);
  f.run(150 * ms, 160 * ms);
  REQUIRE(f.output == std::vector<Event>{{0, "fresh"}});
}

TEST_CASE(
    "Rate limiter default mode retains legacy limit gating and event timestamps",
    "[rate-limiter]")
{
  Limiter f;
  f.node.inputs.quantification.value = 0.;
  f.node.inputs.ms.value = 10;
  f.add(17, "too soon");
  f.run(0, 5 * ms, 240);
  REQUIRE(f.output.empty());
  f.add(11, "first");
  f.add(12, "limited");
  f.run(5 * ms, 10 * ms, 240);
  REQUIRE(f.output == std::vector<Event>{{11, "first"}});
  f.add(19, "next");
  f.run(10 * ms, 20 * ms);
  REQUIRE(f.output == std::vector<Event>{{11, "first"}, {19, "next"}});
  f.run(20 * ms, 30 * ms);
  REQUIRE(f.output.size() == 2);
}

TEST_CASE(
    "Rate limiter legacy zero delay filters slices and emits relative callback "
    "timestamps",
    "[rate-limiter]")
{
  Limiter f;
  f.node.inputs.quantification.value = 0.;
  f.node.inputs.ms.value = 0;
  f.add(0, "before");
  f.add(12, "inside");
  f.add(20, "after");
  f.run(0, 10 * ms, 10, 10);
  REQUIRE(f.output == std::vector<Event>{{2, "inside"}});
}

TEST_CASE(
    "Rate limiter default mode keeps first-grid-point legacy quantization",
    "[rate-limiter]")
{
  Limiter f;
  f.node.inputs.quantification.value = 0.25f;
  f.node.inputs.ms.value = 1000; // Quantization takes precedence over the limit.
  f.add(200, "no grid point");
  f.run(10 * ms, 20 * ms);
  REQUIRE(f.output.empty());

  auto t = Limiter::tick(20 * ms, 30 * ms, 480, 16);
  t.musical_start_position = 0.75;
  t.musical_end_position = 1.25;
  f.add(40, "first");
  f.add(450, "second");
  f.run(t);
  REQUIRE(f.output == std::vector<Event>{{240, "first"}, {240, "second"}});
}

TEST_CASE(
    "Rate limiter host execution carries native dates and adds slice offset once",
    "[rate-limiter]")
{
  ossia::execution_state state;
  state.bufferSize = 512;
  state.modelToSamplesRatio = 48000. / 705600000.;
  state.samplesToModelRatio = 1. / state.modelToSamplesRatio;
  oscr::safe_node<Node> host{512, 48000., 0};
  host.finish_init();
  auto& node = host.impl.effect;
  node.ossia_state = {&state};
  node.inputs.mode.value = Mode::Debounce;
  node.inputs.mode.update(node);
  node.inputs.ms.value = 2;
  node.inputs.port.value->write_value("delayed", 80);
  host.run(Limiter::tick(0, 2 * ms, 96, 32), {&state});
  auto& output = tuplet::get<0>(host.ossia_outlets.ports).data;
  REQUIRE(output.get_data().empty());
  node.inputs.port.value->clear();
  host.run(Limiter::tick(2 * ms, 4 * ms, 96, 128), {&state});
  REQUIRE(output.get_data().size() == 1);
  REQUIRE(output.get_data()[0].timestamp == 176);
  REQUIRE(output.get_data()[0].value == ossia::value{"delayed"});
  output.clear();
  host.run(Limiter::tick(4 * ms, 6 * ms, 96, 224), {&state});
  REQUIRE(output.get_data().empty());
}
