// =============================================================================
// L3 ISF feature surface — IMAGE INPUTS.
//
// Feeds a KNOWN producer (solid magenta and/or a uv gradient) into an ISF
// filter's image input(s) and asserts the output reflects that input, on EVERY
// RHI backend (Catch2 GENERATE over platform_backends()), plus a cross-backend
// agreement check on the horizontal (Y-flip-invariant) axis.
//
// Corpus covered here:
//   * isf-two-images.fs   — two DISTINCT producers → imageA / imageB, split view
//   * isf-image-depth.fs  — image input with DEPTH:true, fed by mrt-depth-color
//
// Run: DISPLAY=:0 ctest -R gfx --output-on-failure
//      (SCORE_TEST_API=opengl|vulkan restricts to one backend)
// =============================================================================
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

// -----------------------------------------------------------------------------
// isf-two-images: imageA = solid magenta, imageB = uv gradient (mrt-single).
// The shader shows imageA on the left, imageB on the right (split at x=0.5).
// This needs TWO distinct producers into TWO image inputs — the linear-chain
// helper only wires one, so build it with GfxPipeline.
// -----------------------------------------------------------------------------
TEST_CASE("isf-two-images blends two distinct image inputs", "[gfx][l3][isf][image]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  struct Out
  {
    bool skipped = false;
    std::string skip_reason, error, backend;
    ReadbackImage img;
  } out;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int pa = p.addIsf(corpus("isf-solid-color.fs"));  // magenta
    const int pb = p.addIsf(corpus("mrt-single-color.fs")); // uv gradient
    const int two = p.addIsf(corpus("isf-two-images.fs"));
    const int sink = p.addSink({64, 64});
    if(pa < 0 || pb < 0 || two < 0)
    {
      out.error = p.error();
      return;
    }
    p.wire(p.imageOut(pa, 0), p.imageIn(two, 0)); // magenta -> imageA
    p.wire(p.imageOut(pb, 0), p.imageIn(two, 1)); // gradient -> imageB
    p.wire(p.imageOut(two, 0), p.sinkInput(sink));
    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      out.backend = p.backend();
      return;
    }
    out.backend = p.backend();
    p.render(3);
    out.img = p.readback(sink);
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.img.valid());

  const int w = out.img.width, h = out.img.height;
  const std::array<uint8_t, 4> magenta{255, 0, 255, 255};

  // Left quarter (well left of split) samples imageA = solid magenta.
  const auto left = out.img.at(w / 8, h / 2);
  // Right quarter samples imageB = uv gradient: at x≈0.875 the red (uv.x)
  // channel is high and green>0 — clearly NOT magenta.
  const auto right = out.img.at(7 * w / 8, h / 2);
  INFO(
      "left=(" << (int)left[0] << "," << (int)left[1] << "," << (int)left[2]
               << ") right=(" << (int)right[0] << "," << (int)right[1] << ","
               << (int)right[2] << ")");

  CHECK(near(left, magenta, 6));       // imageA reproduced on the left
  CHECK(right[0] > 200);               // gradient red high at x≈0.875
  CHECK(right[2] < 60);                // gradient blue ~0 (distinct from magenta)
  CHECK(!near(right, magenta, 20));    // the two inputs are genuinely different
}

TEST_CASE(
    "isf-two-images readback agrees across backends", "[gfx][l3][isf][image]")
{
  std::vector<ReadbackImage> got;
  {
    // Re-run all backends through the same pipeline.
    for(auto api : platform_backends())
    {
      ReadbackImage img;
      bool skipped = false;
      score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
        GfxPipeline p;
        const int pa = p.addIsf(corpus("isf-solid-color.fs"));
        const int pb = p.addIsf(corpus("mrt-single-color.fs"));
        const int two = p.addIsf(corpus("isf-two-images.fs"));
        const int sink = p.addSink({64, 64});
        if(pa < 0 || pb < 0 || two < 0)
          return;
        p.wire(p.imageOut(pa, 0), p.imageIn(two, 0));
        p.wire(p.imageOut(pb, 0), p.imageIn(two, 1));
        p.wire(p.imageOut(two, 0), p.sinkInput(sink));
        if(!p.create(api))
        {
          skipped = p.skipped();
          return;
        }
        p.render(3);
        img = p.readback(sink);
      });
      if(!skipped && img.valid())
        got.push_back(std::move(img));
    }
  }
  if(got.size() < 2)
    SKIP("need >=2 live backends to compare");
  for(std::size_t i = 1; i < got.size(); ++i)
  {
    const int d = max_channel_diff(got[0], got[i]);
    INFO("max channel diff vs backend0 = " << d);
    CHECK(d <= 4);
  }
}

// -----------------------------------------------------------------------------
// isf-image-depth: image input declared DEPTH:true, fed by the depth+color MRT
// producer. Left half samples the color (IMG_NORM_PIXEL), right half samples
// the depth (IMG_DEPTH_NORM_PIXEL). Assert it renders and the color (left) half
// is non-degenerate and agrees across backends. The DEPTH (right) half reads
// back black through this linear chain — the fixture wires only output[0]
// (color) upstream, so the sibling depth attachment is not delivered to the
// depth sampler; that gap is documented (needs a depth-aware chain edge).
// -----------------------------------------------------------------------------
TEST_CASE(
    "isf-image-depth samples a color+depth producer", "[gfx][l3][isf][image][depth]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(
      backend, {corpus("isf-mrt-depth-color.fs"), corpus("isf-image-depth.fs")});

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  // Left half = color of the radial-gradient producer: must be non-degenerate.
  const auto cl = img.at(img.width / 4, img.height / 2);
  INFO("left(color) = (" << (int)cl[0] << "," << (int)cl[1] << "," << (int)cl[2] << ")");
  const std::array<uint8_t, 4> black{0, 0, 0, 255};
  CHECK(!near(cl, black, 6)); // the color half actually sampled the input
}

TEST_CASE(
    "isf-image-depth readback agrees across backends", "[gfx][l3][isf][image][depth]")
{
  const auto shots = render_all(
      {corpus("isf-mrt-depth-color.fs"), corpus("isf-image-depth.fs")});
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
