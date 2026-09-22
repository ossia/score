// The Output port of the path generator is a plain value port: the mode only
// decides what sits inside each ossia::value, and XY must keep emitting exactly
// what the documents saved before the mode existed expect.

#include <PathGenerator/PathGeneratorModel.hpp>

#include <ossia/network/value/value.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace
{
halp::tick_flicks make_tick(double relpos)
{
  halp::tick_flicks tk{};
  tk.frames = 64;
  tk.start_in_flicks = 0;
  tk.end_in_flicks = 705600;
  tk.relative_position = relpos;
  tk.parent_duration = 705600000;
  return tk;
}

//! One source: its first node is the centre, its second the handle.
ossia::value source(ossia::vec2f a, ossia::vec2f b)
{
  return std::vector<ossia::value>{ossia::value{a}, ossia::value{b}};
}

void setup(spat::PathGenerator& g, int sources = 1)
{
  g.inputs.pos.value.clear();
  for(int i = 0; i < sources; i++)
    g.inputs.pos.value.push_back(
        source(ossia::vec2f{0.5f, 0.5f}, ossia::vec2f{0.8f, 0.5f}));
}
}

TEST_CASE("path generator: the output mode picks the value type", "[path][spat]")
{
  spat::PathGenerator g;
  setup(g, 2);
  g.inputs.path.value = spat::Circle;
  g.inputs.z.value = 0.25f;

  SECTION("XY is the default and emits vec2f")
  {
    CHECK(g.inputs.output_mode.value == spat::XY);

    g(make_tick(0.3));
    const auto& out = g.outputs.OutTab.value;
    REQUIRE(out.size() == 2);
    for(const auto& v : out)
      CHECK(v.target<ossia::vec2f>() != nullptr);
  }

  SECTION("XY0 emits vec3f with a zero z")
  {
    g.inputs.output_mode.value = spat::XY0;
    g(make_tick(0.3));

    const auto& out = g.outputs.OutTab.value;
    REQUIRE(out.size() == 2);
    for(const auto& v : out)
    {
      const auto* p = v.target<ossia::vec3f>();
      REQUIRE(p != nullptr);
      CHECK((*p)[2] == Approx(0.f));
    }
  }

  SECTION("XYZ takes its z from the slider")
  {
    g.inputs.output_mode.value = spat::XYZ;
    g(make_tick(0.3));

    const auto& out = g.outputs.OutTab.value;
    REQUIRE(out.size() == 2);
    for(const auto& v : out)
    {
      const auto* p = v.target<ossia::vec3f>();
      REQUIRE(p != nullptr);
      CHECK((*p)[2] == Approx(0.25f));
    }

    // The slider is read every tick, not latched when the mode is picked
    g.inputs.z.value = 0.75f;
    g(make_tick(0.3));
    CHECK((*g.outputs.OutTab.value[0].target<ossia::vec3f>())[2] == Approx(0.75f));
  }
}

TEST_CASE("path generator: x and y do not depend on the mode", "[path][spat]")
{
  spat::PathGenerator g;
  setup(g);
  g.inputs.path.value = spat::Lissajous;

  const auto tick = make_tick(0.42);

  g(tick);
  const ossia::vec2f xy = *g.outputs.OutTab.value[0].target<ossia::vec2f>();

  for(auto mode : {spat::XY0, spat::XYZ})
  {
    g.inputs.output_mode.value = mode;
    g(tick);
    const auto* p = g.outputs.OutTab.value[0].target<ossia::vec3f>();
    REQUIRE(p != nullptr);
    CHECK((*p)[0] == xy[0]);
    CHECK((*p)[1] == xy[1]);
  }

  // ... and back: an XY document reloaded next to an XYZ one is untouched
  g.inputs.output_mode.value = spat::XY;
  g(tick);
  const auto* p = g.outputs.OutTab.value[0].target<ossia::vec2f>();
  REQUIRE(p != nullptr);
  CHECK((*p)[0] == xy[0]);
  CHECK((*p)[1] == xy[1]);
}

TEST_CASE("path generator: the steady state does not reallocate", "[path][spat]")
{
  spat::PathGenerator g;
  setup(g, 4);
  g.inputs.path.value = spat::Spiral;

  const auto run = [&](double u) { g(make_tick(u)); };

  run(0.);
  const auto& out = g.outputs.OutTab.value;
  const auto* data = out.data();
  const auto capacity = out.capacity();

  for(int i = 1; i < 128; i++)
    run(double(i) / 128.);

  CHECK(out.data() == data);
  CHECK(out.capacity() == capacity);

  // Switching the mode rewrites the values in place: the buffer is the same one
  g.inputs.output_mode.value = spat::XYZ;
  run(0.5);
  CHECK(out.data() == data);
  CHECK(out.capacity() == capacity);

  for(int i = 1; i < 128; i++)
    run(double(i) / 128.);
  CHECK(out.data() == data);
  CHECK(out.capacity() == capacity);
}
