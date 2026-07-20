// =============================================================================
// L3 ISF CONTROL-VALUE injection (Priority 2 inputs — the previously-skipped
// float / color / point2D / point3D / bool / long control-inlet families).
//
// An ISF float/color/point/bool/long INPUT becomes a *control* inlet whose value
// is packed into the node's std140 material UBO (ISFNode.cpp isf_input_port_vis).
// In the running app the exec engine pushes those values as a score::gfx::Message;
// here the fixture's setControl() / render_isf_controls() drive the SAME public
// entry point (ProcessNode::process(port, ossia::value) -> writes the material
// slot + bumps materialChange), so the shader sees an injected value with no
// engine changes. Each corpus shader emits its control VERBATIM, so the readback
// is analytic: the offscreen target is a plain non-sRGB RGBA8, hence a component
// c in [0,1] reads back as round(255*c).
//
// This is the mechanism that closes the "control-value inputs are not injectable"
// SKIP in IsfUnsupported.cpp for the float/color/point2D/point3D/bool/long types.
//
// Every case iterates platform_backends() and asserts per backend, plus a
// cross-backend agreement check (the whole point of L3).
//
// Run: DISPLAY=:0 ctest -R gfx_control --output-on-failure
// =============================================================================
#include "IsfTestCommon.hpp"

#include <ossia/network/value/value.hpp>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
/// Render a single ISF node with injected control values on ONE backend inside a
/// booted GUI app; collect the readback only (Catch2 macros run afterwards).
IsfResult control_render(
    score::gfx::GraphicsApi backend, const QString& path,
    std::vector<ControlSetting> controls, QSize size = {64, 64}, int frames = 3)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_controls(backend, path, std::move(controls), size, frames);
  });
  return r;
}

/// The center pixel of a solid-color control render, or {0,0,0,0} on failure.
std::array<uint8_t, 4> center_of(const IsfResult& r)
{
  if(!r.outputs.empty() && r.outputs[0].valid())
    return r.outputs[0].center();
  return {0, 0, 0, 0};
}

/// Assert every sampled pixel of a *solid* control render equals `expect`.
void check_uniform(const ReadbackImage& img, std::array<uint8_t, 4> expect, int tol)
{
  REQUIRE(img.valid());
  for(int y = 2; y < img.height - 2; y += 5)
    for(int x = 2; x < img.width - 2; x += 5)
    {
      const auto p = img.at(x, y);
      INFO("pixel (" << x << "," << y << ") = (" << (int)p[0] << "," << (int)p[1]
                     << "," << (int)p[2] << "," << (int)p[3] << ") expect ("
                     << (int)expect[0] << "," << (int)expect[1] << ","
                     << (int)expect[2] << "," << (int)expect[3] << ")");
      CHECK(near(p, expect, tol));
    }
}
} // namespace

// -----------------------------------------------------------------------------
// COLOR control inlet: inject col=(r,g,b,a) -> output == round(255*col).
// -----------------------------------------------------------------------------
TEST_CASE("ISF color control inlet is injectable and analytic", "[gfx][l3][isf][control]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  // A value that is NOT the shader DEFAULT (magenta) and has distinct channels.
  const IsfResult r = control_render(
      backend, corpus("isf-control-color.fs"),
      {{0, ossia::value{ossia::vec4f{0.2f, 0.4f, 0.6f, 1.0f}}}});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);

  // round(255 * (0.2,0.4,0.6,1.0)) = (51,102,153,255). Tol 2 for LSB rounding.
  check_uniform(r.outputs[0], {51, 102, 153, 255}, 2);
}

TEST_CASE("ISF color control default differs from injected", "[gfx][l3][isf][control]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  // No injection -> ISF DEFAULT magenta (1,0,1,1).
  const IsfResult def = control_render(backend, corpus("isf-control-color.fs"), {});
  if(def.skipped)
    SKIP(def.backend + ": " + def.skip_reason);
  REQUIRE(def.error.empty());
  REQUIRE(def.outputs.size() == 1);
  check_uniform(def.outputs[0], {255, 0, 255, 255}, 2);

  // Injecting green must actually change the output (proves injection is live).
  const IsfResult inj = control_render(
      backend, corpus("isf-control-color.fs"),
      {{0, ossia::value{ossia::vec4f{0.0f, 1.0f, 0.0f, 1.0f}}}});
  REQUIRE(inj.error.empty());
  check_uniform(inj.outputs[0], {0, 255, 0, 255}, 2);
}

TEST_CASE("ISF color control agrees across backends", "[gfx][l3][isf][control]")
{
  std::vector<ReadbackImage> got;
  for(auto api : platform_backends())
  {
    const IsfResult r = control_render(
        api, corpus("isf-control-color.fs"),
        {{0, ossia::value{ossia::vec4f{0.2f, 0.4f, 0.6f, 1.0f}}}});
    if(!r.skipped && r.error.empty() && !r.outputs.empty() && r.outputs[0].valid())
      got.push_back(r.outputs[0]);
  }
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int d = max_channel_diff(got[0], got[i]);
    INFO("max channel diff vs backend0 = " << d);
    CHECK(d <= 2);
  }
}

// -----------------------------------------------------------------------------
// FLOAT control inlet: inject level=v -> grayscale round(255*v).
// -----------------------------------------------------------------------------
TEST_CASE("ISF float control inlet is injectable and analytic", "[gfx][l3][isf][control]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = control_render(
      backend, corpus("isf-control-float.fs"), {{0, ossia::value{0.5f}}});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  // round(255 * 0.5) = 128 (0.5*255=127.5 -> 128 or 127 depending on rounding).
  check_uniform(r.outputs[0], {128, 128, 128, 255}, 2);
}

TEST_CASE("ISF float control sweeps monotonically", "[gfx][l3][isf][control]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult lo = control_render(
      backend, corpus("isf-control-float.fs"), {{0, ossia::value{0.0f}}});
  if(lo.skipped)
    SKIP(lo.backend + ": " + lo.skip_reason);
  const IsfResult mid = control_render(
      backend, corpus("isf-control-float.fs"), {{0, ossia::value{0.25f}}});
  const IsfResult hi = control_render(
      backend, corpus("isf-control-float.fs"), {{0, ossia::value{1.0f}}});
  REQUIRE(lo.error.empty());
  REQUIRE(mid.error.empty());
  REQUIRE(hi.error.empty());

  const int vlo = center_of(lo)[0];
  const int vmid = center_of(mid)[0];
  const int vhi = center_of(hi)[0];
  INFO("lo=" << vlo << " mid=" << vmid << " hi=" << vhi);
  CHECK(vlo <= 2);
  CHECK(std::abs(vmid - 64) <= 2);
  CHECK(vhi >= 253);
}

// -----------------------------------------------------------------------------
// POINT2D control inlet: inject pt=(x,y) -> (round(255*x), round(255*y), 0, 255).
// -----------------------------------------------------------------------------
TEST_CASE("ISF point2D control inlet is injectable and analytic", "[gfx][l3][isf][control]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = control_render(
      backend, corpus("isf-control-point2d.fs"),
      {{0, ossia::value{ossia::vec2f{0.25f, 0.75f}}}});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  // (round(255*0.25), round(255*0.75), 0, 255) = (64, 191, 0, 255).
  check_uniform(r.outputs[0], {64, 191, 0, 255}, 2);
}

// -----------------------------------------------------------------------------
// POINT3D control inlet: inject p3=(x,y,z) -> (255x, 255y, 255z, 255).
// -----------------------------------------------------------------------------
TEST_CASE("ISF point3D control inlet is injectable and analytic", "[gfx][l3][isf][control]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = control_render(
      backend, corpus("isf-control-point3d.fs"),
      {{0, ossia::value{ossia::vec3f{0.2f, 0.5f, 0.8f}}}});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  // (round(255*0.2), round(255*0.5), round(255*0.8), 255) = (51, 128, 204, 255).
  check_uniform(r.outputs[0], {51, 128, 204, 255}, 2);
}

// -----------------------------------------------------------------------------
// BOOL control inlet: inject on=true -> white, on=false -> black.
// -----------------------------------------------------------------------------
TEST_CASE("ISF bool control inlet is injectable", "[gfx][l3][isf][control]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult t = control_render(
      backend, corpus("isf-control-bool.fs"), {{0, ossia::value{true}}});
  if(t.skipped)
    SKIP(t.backend + ": " + t.skip_reason);
  const IsfResult f = control_render(
      backend, corpus("isf-control-bool.fs"), {{0, ossia::value{false}}});
  REQUIRE(t.error.empty());
  REQUIRE(f.error.empty());
  REQUIRE(t.outputs.size() == 1);
  REQUIRE(f.outputs.size() == 1);
  check_uniform(t.outputs[0], {255, 255, 255, 255}, 2);
  check_uniform(f.outputs[0], {0, 0, 0, 255}, 2);
}

// -----------------------------------------------------------------------------
// LONG control inlet: inject sel=v (0..10) -> grayscale round(255*v/10).
// -----------------------------------------------------------------------------
TEST_CASE("ISF long control inlet is injectable and analytic", "[gfx][l3][isf][control]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = control_render(
      backend, corpus("isf-control-long.fs"), {{0, ossia::value{5}}});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  // sel=5 -> 5/10 = 0.5 -> round(255*0.5) = 128.
  check_uniform(r.outputs[0], {128, 128, 128, 255}, 2);

  // A different value must produce a different (analytic) level.
  const IsfResult r2 = control_render(
      backend, corpus("isf-control-long.fs"), {{0, ossia::value{2}}});
  REQUIRE(r2.error.empty());
  // sel=2 -> 0.2 -> 51.
  check_uniform(r2.outputs[0], {51, 51, 51, 255}, 2);
}

// -----------------------------------------------------------------------------
// TWO control inlets interacting (color 'base' * float 'gain'): exercises std140
// packing of a vec4 followed by a float, asserting BOTH land at the right offset.
// -----------------------------------------------------------------------------
TEST_CASE("ISF two control inlets (color*gain) pack and inject correctly", "[gfx][l3][isf][control]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  // base=(0.8,0.4,0.2,1), gain=0.5 -> rgb = round(255*base*0.5) = (102,51,26).
  const IsfResult r = control_render(
      backend, corpus("isf-control-mix.fs"),
      {{0, ossia::value{ossia::vec4f{0.8f, 0.4f, 0.2f, 1.0f}}},
       {1, ossia::value{0.5f}}});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  check_uniform(r.outputs[0], {102, 51, 26, 255}, 3);
}
