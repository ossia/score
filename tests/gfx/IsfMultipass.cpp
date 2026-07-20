// =============================================================================
// L3 ISF feature surface — MULTIPASS.
//
// Renders multi-pass ISF shaders (PASSES with intermediate TARGETs) and asserts
// the final composite, on every RHI backend, plus cross-backend agreement.
//
// Corpus covered here (all render correctly on GL + Vulkan):
//   * isf-three-pass.fs        — 3 passes; final R=uv.x, G=uv.y, B=(x+y)/2.
//   * isf-multipass-size.fs    — pass 0 into a fixed 64x64 TARGET, pass 1 samples
//                                it; asserts it renders structure at output dims.
//   * isf-multipass-expr-size.fs — pass 0 into a $WIDTH/2 x $HEIGHT/2 TARGET.
//
// The multipass STORAGE / PERSISTENT-SSBO variants
// (isf-multipass-storage-rw.fs, isf-multipass-persistent-ssbo.fs) render an
// all-black final pass on BOTH backends here (a real finding — the final pass
// output never reaches the sink for the storage/persistent multipass path,
// while non-storage multipass such as three-pass works). They are kept as
// honest RED findings in the isolated target test_gfx_isf_findings.
//
// Run: DISPLAY=:0 ctest -R gfx --output-on-failure
// =============================================================================
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
// Assert the final readback of `chain` agrees across all live backends.
void check_cross_backend(std::vector<QString> chain)
{
  const auto shots = render_all(std::move(chain));
  std::vector<ReadbackImage> got;
  for(const auto& s : shots)
    if(!s.result.skipped && s.result.error.empty() && !s.result.outputs.empty()
       && s.result.outputs[0].valid())
      got.push_back(s.result.outputs[0]);
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int d = max_channel_diff(got[0], got[i]);
    INFO("max channel diff vs backend0 = " << d);
    CHECK(d <= 4);
  }
}
}

// -----------------------------------------------------------------------------
// isf-three-pass: analytic final composite.
//   pass0 -> buf0 = (uv.x,0,0);  pass1 -> buf1 = (buf0.r*.5, uv.y, 0);
//   pass2 (final)  = (buf0.r=uv.x, buf1.g=uv.y, (uv.x+uv.y)*.5, 1).
// RED tracks uv.x (X gradient, orientation-invariant); centre ~ (128,128,128).
// -----------------------------------------------------------------------------
TEST_CASE("isf-three-pass composites its intermediate targets", "[gfx][l3][isf][multipass]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-three-pass.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto c = img.center();
  INFO("centre = (" << (int)c[0] << "," << (int)c[1] << "," << (int)c[2] << ")");
  CHECK(near(c, {128, 128, 128, 255}, 12)); // uv=(.5,.5)

  // RED = uv.x rises left->right (proves buf0 survived to the final pass).
  const int rL = img.at(img.width / 8, img.height / 2)[0];
  const int rR = img.at(7 * img.width / 8, img.height / 2)[0];
  INFO("RED left=" << rL << " right=" << rR);
  CHECK(rL < 60);
  CHECK(rR > 200);
}

TEST_CASE("isf-three-pass agrees across backends", "[gfx][l3][isf][multipass]")
{
  check_cross_backend({corpus("isf-three-pass.fs")});
}

// -----------------------------------------------------------------------------
// isf-multipass-size: pass 0 renders a checker+gradient into a fixed 64x64
// TARGET, pass 1 samples it. Assert the final render is non-degenerate (the
// intermediate TARGET was created, written and sampled) and dims == sink dims.
// -----------------------------------------------------------------------------
TEST_CASE("isf-multipass-size renders through a fixed-size pass", "[gfx][l3][isf][multipass]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-multipass-size.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());
  CHECK(img.width == 64);
  CHECK(img.height == 64);
  CHECK(non_degenerate(img)); // the intermediate pass produced real structure
}

TEST_CASE("isf-multipass-size agrees across backends", "[gfx][l3][isf][multipass]")
{
  check_cross_backend({corpus("isf-multipass-size.fs")});
}

// -----------------------------------------------------------------------------
// isf-multipass-expr-size: pass 0 into a $WIDTH/2 x $HEIGHT/2 TARGET (expression
// sizing), pass 1 samples it. Assert it renders non-degenerate structure and
// agrees across backends (proves the WIDTH/HEIGHT expressions evaluated).
// -----------------------------------------------------------------------------
TEST_CASE("isf-multipass-expr-size renders through an expression-sized pass", "[gfx][l3][isf][multipass]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-multipass-expr-size.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());
  CHECK(non_degenerate(img));
}

TEST_CASE("isf-multipass-expr-size agrees across backends", "[gfx][l3][isf][multipass]")
{
  check_cross_backend({corpus("isf-multipass-expr-size.fs")});
}
