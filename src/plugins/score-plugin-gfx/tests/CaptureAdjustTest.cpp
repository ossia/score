// Unit tests for the capture-side corrections: the maths, the UBO layout the
// shader reads them through, and the cross-thread handoff that carries them
// from a control write to the render thread.
//
// No GPU and no camera, so this runs anywhere. What it cannot check is that the
// GLSL in CaptureAdjustGLSL.hpp matches adjustCaptureReference -- only hardware
// can say that -- so the reference is written as the specification and the
// shader mirrors it step for step.

#include <Gfx/CaptureControlTree.hpp>
#include <Gfx/Graph/CaptureAdjust.hpp>

#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <limits>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using namespace score::gfx;

namespace
{
using Catch::Approx;

/// Convenience: run the reference over one colour.
struct Rgb
{
  float v[3];
};
Rgb adjusted(const CaptureAdjust& a, float r, float g, float b)
{
  Rgb out{{r, g, b}};
  adjustCaptureReference(a, out.v);
  return out;
}

TEST_CASE("capture adjust: defaults are the identity", "[gfx][capture]")
{
  // The whole point of the defaults: a pipeline that sets nothing must render
  // what it rendered before any of this existed.
  const CaptureAdjust a{};
  for(float v : {0.f, 0.25f, 0.5f, 0.75f, 1.f})
  {
    const auto out = adjusted(a, v, v, v);
    INFO("identity r at " + std::to_string(v));
  REQUIRE(out.v[0] == Approx(v).margin(1e-5f));
    INFO("identity g at " + std::to_string(v));
  REQUIRE(out.v[1] == Approx(v).margin(1e-5f));
    INFO("identity b at " + std::to_string(v));
  REQUIRE(out.v[2] == Approx(v).margin(1e-5f));
  }
}

TEST_CASE("capture adjust: black level", "[gfx][capture]")
{
  CaptureAdjust a{};
  a.blackLevel[0] = a.blackLevel[1] = a.blackLevel[2] = 0.2f;

  // The pedestal maps to zero...
  const auto atBlack = adjusted(a, 0.2f, 0.2f, 0.2f);
  INFO("black level maps the pedestal to 0");
  REQUIRE(atBlack.v[0] == Approx(0.f).margin(1e-5f));

  // ...white stays white, because the remaining range is renormalised rather
  // than merely shifted. A shift would darken every highlight too.
  const auto atWhite = adjusted(a, 1.f, 1.f, 1.f);
  INFO("black level keeps white at white");
  REQUIRE(atWhite.v[0] == Approx(1.f).margin(1e-5f));

  // Below the pedestal clamps rather than going negative.
  const auto below = adjusted(a, 0.1f, 0.1f, 0.1f);
  INFO("below the pedestal clamps to 0");
  REQUIRE(below.v[0] == Approx(0.f).margin(1e-5f));
}

TEST_CASE("capture adjust: white balance", "[gfx][capture]")
{
  CaptureAdjust a{};
  a.whiteBalance[0] = 2.f;
  a.whiteBalance[2] = 0.5f;
  const auto out = adjusted(a, 0.25f, 0.25f, 0.25f);
  INFO("wb red gain");
  REQUIRE(out.v[0] == Approx(0.5f).margin(1e-5f));
  INFO("wb green untouched");
  REQUIRE(out.v[1] == Approx(0.25f).margin(1e-5f));
  INFO("wb blue gain");
  REQUIRE(out.v[2] == Approx(0.125f).margin(1e-5f));
}

TEST_CASE("capture adjust: pedestal is removed before the gain", "[gfx][capture]")
{
  // Ordering matters: gain applied before the pedestal is removed would scale
  // the pedestal too, and the result differs. Pin the order down.
  CaptureAdjust a{};
  a.blackLevel[0] = 0.5f;
  a.whiteBalance[0] = 2.f;

  // Correct order: (0.75 - 0.5) / 0.5 = 0.5, then x2 = 1.0
  const auto out = adjusted(a, 0.75f, 0.f, 0.f);
  INFO("black level is removed before the gain");
  REQUIRE(out.v[0] == Approx(1.f).margin(1e-5f));

  // The wrong order would be (0.75 * 2 - 0.5) / 0.5 = 2.0 -> clamped to 1.0,
  // which happens to agree here, so check a value where it does not: 0.6.
  // right: (0.6-0.5)/0.5 = 0.2, x2 = 0.4.  wrong: (1.2-0.5)/0.5 = 1.4 -> 1.0
  const auto out2 = adjusted(a, 0.6f, 0.f, 0.f);
  INFO("ordering: pedestal first, then gain");
  REQUIRE(out2.v[0] == Approx(0.4f).margin(1e-5f));
}

TEST_CASE("capture adjust: exposure", "[gfx][capture]")
{
  CaptureAdjust a{};
  a.exposure = 4.f;
  const auto out = adjusted(a, 0.1f, 0.1f, 0.1f);
  INFO("exposure multiplies linearly");
  REQUIRE(out.v[0] == Approx(0.4f).margin(1e-5f));

  const auto clipped = adjusted(a, 0.5f, 0.5f, 0.5f);
  INFO("exposure clips at white");
  REQUIRE(clipped.v[0] == Approx(1.f).margin(1e-5f));
}

TEST_CASE("capture adjust: transfer curve", "[gfx][capture]")
{
  CaptureAdjust a{};
  a.gamma = 2.2f;
  const auto out = adjusted(a, 0.5f, 0.5f, 0.5f);
  INFO("gamma raises to 1/gamma");
  REQUIRE(out.v[0] == Approx(std::pow(0.5f, 1.f / 2.2f)).margin(1e-5f));

  // The endpoints are fixed points of any exponent, which is what keeps a
  // transfer curve from shifting black or white.
  INFO("gamma fixes black");
  REQUIRE(adjusted(a, 0.f, 0.f, 0.f).v[0] == Approx(0.f).margin(1e-5f));
  INFO("gamma fixes white");
  REQUIRE(adjusted(a, 1.f, 1.f, 1.f).v[0] == Approx(1.f).margin(1e-5f));

  // gamma = 1 must be exactly the identity, not merely close: it is the default
  // and any drift would restyle every existing project.
  CaptureAdjust one{};
  INFO("gamma 1 is the identity");
  REQUIRE(adjusted(one, 0.37f, 0.37f, 0.37f).v[0] == Approx(0.37f).margin(1e-6f));
}

TEST_CASE("capture adjust: saturation", "[gfx][capture]")
{
  CaptureAdjust a{};
  a.saturation = 0.f;
  const auto grey = adjusted(a, 1.f, 0.f, 0.f);
  const float lumaOfRed = 0.2126f;
  INFO("saturation 0 collapses to luma r");
  REQUIRE(grey.v[0] == Approx(lumaOfRed).margin(1e-5f));
  INFO("saturation 0 collapses to luma g");
  REQUIRE(grey.v[1] == Approx(lumaOfRed).margin(1e-5f));
  INFO("saturation 0 collapses to luma b");
  REQUIRE(grey.v[2] == Approx(lumaOfRed).margin(1e-5f));

  CaptureAdjust keep{};
  const auto same = adjusted(keep, 0.8f, 0.4f, 0.1f);
  INFO("saturation 1 leaves colour alone");
  REQUIRE(same.v[0] == Approx(0.8f).margin(1e-5f));
  INFO("saturation 1 leaves colour alone");
  REQUIRE(same.v[2] == Approx(0.1f).margin(1e-5f));

  // A grey input is unchanged by any saturation, since it is already its luma.
  CaptureAdjust boost{};
  boost.saturation = 2.f;
  INFO("saturation does not move grey");
  REQUIRE(adjusted(boost, 0.5f, 0.5f, 0.5f).v[0] == Approx(0.5f).margin(1e-5f));
}

TEST_CASE("capture adjust: degenerate settings never produce NaN", "[gfx][capture]")
{
  // A control can be driven to a value that would divide by zero or raise to
  // an infinite power. None of it may produce NaN: a NaN texel propagates
  // through blending and takes the whole frame with it.
  CaptureAdjust a{};
  a.blackLevel[0] = a.blackLevel[1] = a.blackLevel[2] = 1.f; // full pedestal
  a.gamma = 0.f;                                            // degenerate curve
  const auto out = adjusted(a, 0.5f, 0.5f, 0.5f);
  for(int i = 0; i < 3; ++i)
  {
    INFO("degenerate settings must not produce NaN");
  REQUIRE(!std::isnan(out.v[i]));
    INFO("degenerate settings must stay in range");
  REQUIRE((out.v[i] >= 0.f && out.v[i] <= 1.f));
  }
}

TEST_CASE("capture adjust: the shader reads the block at the right offsets", "[gfx][capture]")
{
  // The shader reads this buffer by offset; if the packing drifts the picture
  // gets someone else's numbers.
  CaptureMaterialUBO u{};
  const auto* base = reinterpret_cast<const char*>(&u);
  INFO("scale at 0");
  REQUIRE(reinterpret_cast<const char*>(&u.scale[0]) - base == 0);
  INFO("textureSize at 8");
  REQUIRE(reinterpret_cast<const char*>(&u.textureSize[0]) - base == 8);
  INFO("blackLevel at 16");
  REQUIRE(reinterpret_cast<const char*>(&u.blackLevel[0]) - base == 16);
  INFO("whiteBalance at 32");
  REQUIRE(reinterpret_cast<const char*>(&u.whiteBalance[0]) - base == 32);
  INFO("params at 48");
  REQUIRE(reinterpret_cast<const char*>(&u.params[0]) - base == 48);
  INFO("the block is 64 bytes");
  REQUIRE(sizeof(CaptureMaterialUBO) == 64);

  // Defaults in the buffer must be the identity too, because a node that never
  // receives a control write still uploads this.
  INFO("default black level is 0");
  REQUIRE(u.blackLevel[0] == 0.f);
  INFO("default white balance is 1");
  REQUIRE(u.whiteBalance[0] == 1.f);
  INFO("default exposure/gamma/saturation are 1");
  REQUIRE((u.params[0] == 1.f && u.params[1] == 1.f && u.params[2] == 1.f));

  CaptureAdjust a{};
  a.blackLevel[1] = 0.125f;
  a.whiteBalance[2] = 1.5f;
  a.exposure = 2.f;
  a.gamma = 2.2f;
  a.saturation = 0.5f;
  applyTo(a, u);
  INFO("applyTo carries black level");
  REQUIRE(u.blackLevel[1] == 0.125f);
  INFO("applyTo carries white balance");
  REQUIRE(u.whiteBalance[2] == 1.5f);
  INFO("applyTo carries exposure");
  REQUIRE(u.params[0] == 2.f);
  INFO("applyTo carries gamma");
  REQUIRE(u.params[1] == 2.2f);
  INFO("applyTo carries saturation");
  REQUIRE(u.params[2] == 0.5f);
  INFO("applyTo leaves the geometry half alone");
  REQUIRE(u.scale[0] == 1.f);
}

TEST_CASE("capture adjust: the slot publishes each change once", "[gfx][capture]")
{
  CaptureAdjustSlot slot;
  CaptureAdjust seen{};
  std::uint32_t gen = 0;

  // Nothing published yet: the reader must not be told to do work.
  INFO("a fresh slot reports no change");
  REQUIRE(!slot.poll(seen, gen));

  CaptureAdjust a{};
  a.gamma = 2.2f;
  slot.set(a);
  INFO("a write is seen once");
  REQUIRE(slot.poll(seen, gen));
  INFO("the written value arrives");
  REQUIRE(seen.gamma == Approx(2.2f).margin(1e-6f));
  INFO("and is not seen twice");
  REQUIRE(!slot.poll(seen, gen));

  // Writing the same value must not wake the renderer: a control that re-emits
  // its current value on every UI tick would otherwise rebuild the buffer at
  // frame rate for nothing.
  slot.set(a);
  INFO("an identical write is not a change");
  REQUIRE(!slot.poll(seen, gen));

  a.gamma = 1.8f;
  slot.set(a);
  INFO("a different write is a change");
  REQUIRE(slot.poll(seen, gen));
  INFO("the newer value arrives");
  REQUIRE(seen.gamma == Approx(1.8f).margin(1e-6f));
}

TEST_CASE("capture adjust: the slot never tears across threads", "[gfx][capture]")
{
  // The writer is a control callback on whatever thread it arrived on; the
  // reader is the render thread. Neither may tear or wedge.
  CaptureAdjustSlot slot;
  std::atomic_bool stop{false};
  std::atomic_int reads{0};

  std::thread writer{[&] {
    for(int i = 0; i < 20000; ++i)
    {
      CaptureAdjust a{};
      // Two fields moving together: a torn read would pair one's new value
      // with the other's old one, which is exactly what the lock prevents.
      a.gamma = 1.f + float(i % 100) / 100.f;
      a.exposure = a.gamma;
      slot.set(a);
    }
    stop = true;
  }};

  CaptureAdjust seen{};
  std::uint32_t gen = 0;
  while(!stop.load())
  {
    if(slot.poll(seen, gen))
    {
      ++reads;
      INFO("a poll must never pair fields from different writes");
  REQUIRE(seen.exposure == Approx(seen.gamma).margin(1e-6f));
    }
  }
  writer.join();
  slot.poll(seen, gen);

  INFO("the reader saw at least one publication");
  REQUIRE(reads.load() > 0);
  std::printf("  slot: %d publications observed\n", reads.load());
}

/// The /render/ group, over a slot nothing else is driving.
struct RenderFixture
{
  ossia::net::generic_device dev{
      std::make_unique<ossia::net::multiplex_protocol>(), "cam"};
  score::gfx::CaptureAdjustSlot slot;
  Gfx::CaptureControlTree tree{slot, dev, dev.get_root_node()};

  ossia::net::parameter_base* param(const std::string& name)
  {
    auto* n = ossia::net::find_node(dev.get_root_node(), "/render/" + name);
    return n ? n->get_parameter() : nullptr;
  }
};

TEST_CASE("render tree: every declared control is addressable", "[gfx][capture]")
{
  RenderFixture f;
  for(const char* name :
      {"scale_mode", "black_level", "white_balance", "exposure", "gamma",
       "saturation"})
  {
    INFO(std::string{"/render/"} + name);
    REQUIRE(f.param(name) != nullptr);
  }
  REQUIRE(f.tree.count() == 6);
}

TEST_CASE("render tree: the tree starts at the slot's values", "[gfx][capture]")
{
  // If the published defaults disagreed with the slot, the first frame would
  // render one thing and the explorer would claim another.
  RenderFixture f;
  const auto a = f.slot.value();
  REQUIRE(ossia::convert<float>(f.param("gamma")->value()) == Approx(a.gamma));
  REQUIRE(
      ossia::convert<float>(f.param("exposure")->value()) == Approx(a.exposure));
  const ossia::value mode = f.param("scale_mode")->value();
  REQUIRE(mode.target<std::string>() != nullptr);
  // Stretch is the pre-existing behaviour, so it is the default; see
  // CaptureAdjust::scaleMode.
  REQUIRE(*mode.target<std::string>() == "Stretch");
}

TEST_CASE("render tree: a write reaches the slot", "[gfx][capture]")
{
  RenderFixture f;
  f.param("gamma")->push_value(2.2f);
  REQUIRE(f.slot.value().gamma == Approx(2.2f));

  f.param("exposure")->push_value(3.f);
  const auto a = f.slot.value();
  REQUIRE(a.exposure == Approx(3.f));
  // The earlier edit must survive the later one: each control owns one field,
  // so a write is a read-modify-write of the whole struct.
  REQUIRE(a.gamma == Approx(2.2f));
}

TEST_CASE("render tree: vec3 controls land per channel", "[gfx][capture]")
{
  RenderFixture f;
  f.param("white_balance")->push_value(ossia::vec3f{2.f, 1.f, 0.5f});
  const auto a = f.slot.value();
  REQUIRE(a.whiteBalance[0] == Approx(2.f));
  REQUIRE(a.whiteBalance[1] == Approx(1.f));
  REQUIRE(a.whiteBalance[2] == Approx(0.5f));

  f.param("black_level")->push_value(ossia::vec3f{0.1f, 0.2f, 0.3f});
  const auto b = f.slot.value();
  REQUIRE(b.blackLevel[2] == Approx(0.3f));
  REQUIRE(b.whiteBalance[0] == Approx(2.f));
}

TEST_CASE("render tree: scale mode maps by name", "[gfx][capture]")
{
  RenderFixture f;
  const std::pair<const char*, score::gfx::ScaleMode> cases[]{
      {"Original", score::gfx::ScaleMode::Original},
      {"BlackBars", score::gfx::ScaleMode::BlackBars},
      {"Fill", score::gfx::ScaleMode::Fill},
      {"Stretch", score::gfx::ScaleMode::Stretch}};
  for(const auto& [name, mode] : cases)
  {
    INFO(name);
    f.param("scale_mode")->push_value(std::string{name});
    REQUIRE(f.slot.value().scaleMode == mode);
  }

  // An unknown name must not silently pick something: it leaves the mode alone.
  f.param("scale_mode")->push_value(std::string{"Fill"});
  f.param("scale_mode")->push_value(std::string{"NotAMode"});
  REQUIRE(f.slot.value().scaleMode == score::gfx::ScaleMode::Fill);
}

TEST_CASE("render tree: a write moves the generation exactly once",
          "[gfx][capture]")
{
  // The renderer rebuilds its buffer on a generation change, so a control that
  // re-emits an unchanged value must not make it rebuild every frame.
  RenderFixture f;
  const auto before = f.slot.generation();
  f.param("gamma")->push_value(1.5f);
  const auto after = f.slot.generation();
  REQUIRE(after == before + 1);

  f.param("gamma")->push_value(1.5f);
  REQUIRE(f.slot.generation() == after);
}


TEST_CASE("render tree: a non-finite write is refused", "[gfx][capture]")
{
  // A domain describes values rather than enforcing them, so an OSC client can
  // send anything. NaN survives every clamp in the shader -- it compares false
  // against everything -- so it must not reach the slot at all.
  RenderFixture f;
  f.param("gamma")->push_value(2.0f);
  f.param("gamma")->push_value(std::numeric_limits<float>::quiet_NaN());
  REQUIRE(f.slot.value().gamma == Approx(2.0f));

  f.param("exposure")->push_value(std::numeric_limits<float>::infinity());
  REQUIRE(std::isfinite(f.slot.value().exposure));
}
} // namespace
