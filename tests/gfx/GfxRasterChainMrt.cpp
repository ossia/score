// =============================================================================
// P0-7 -- A RAW-RASTER PIPELINE FEEDING A SECOND, MRT RAW-RASTER PIPELINE
// SURVIVES (and produces the right pixels, and survives a sink resize).
//
// Intended registration (tests/gfx/CMakeLists.txt):
//   score_add_gfx_test(raster_chain_mrt GfxRasterChainMrt.cpp)
//
// This is the topology of the user crash score
// 2026/crash-renderpipeline-mrt.score:
//
//   CSF geometry --Geometry--> raw-raster #1 (1 FRAGMENT_OUTPUT)
//                                   |  Image
//                                   v
//   CSF geometry --Geometry--> raw-raster #2 (2 FRAGMENT_OUTPUTS, m_hasMRT)
//                                   |  Image x2
//                                   v
//                              sink0, sink1 (one BackgroundNode per attachment)
//
// Stage #1 (syn-rr-chain-src.fs) writes a Y-INVARIANT closed form:
// (X ramp, 0.25, 0.75, 1). Stage #2 (syn-rr-chain-mrt.fs) samples it:
//   attachment 0 = IMG_NORM_PIXEL(tex, v_uv)  -> expected (X ramp, 64, 191, 255)
//     The constant G/B markers can only come from a LIVE upstream texture --
//     a black/empty/unbound sampler reads (0,0,0,*), and stage #2's own form
//     has B = 128 -- so this pins that the raster->raster image edge actually
//     carries stage #1's output. The upstream form being Y-invariant makes the
//     check independent of the intermediate render target's vertical
//     orientation (only the FINAL sink orientation is a pinned contract, per
//     IsfTestCommon.hpp / GfxOrientationMatrix.cpp).
//   attachment 1 = (X ramp, Y ramp with 255 at the TOP row, 128, 255)
//     Stage #2's own closed form in the pinned orientation, proving the second
//     attachment is really the second FRAGMENT_OUTPUT (same convention as
//     GfxRawRasterMrtPattern.cpp).
// After the initial 3-frame render + readback, sink0 is resized 64x64 -> 96x64
// (GfxPipeline::resizeSink -> BackgroundNode::setSize -> the Graph's onResize
// -> Graph::recreateOutputRenderList, a teardown + rebuild of that output's
// RenderList) and both sinks must read back correctly again -- sink0 at the
// new size, sink1 untouched at 64x64.
//
// ENGINE SURFACE DRIVEN (src/plugins/score-plugin-gfx/Gfx/Graph/
// RenderedRawRasterPipelineNode.cpp unless noted):
//   * m_hasMRT decision: line 2023-2024
//     ("m_hasMRT = colorCount > 1 || hasDepth || hasLayered || hasCubemap ||
//     multiview_count >= 2") -- true for stage #2 (2 colour outputs), false
//     for stage #1.
//   * Stage #2's input sampler: initState -> line 1693
//     "m_inputSamplers = initInputSamplers(this->n, renderer, n.input, ...)";
//     Utils.cpp:1376 initInputSamplers resolves a connected image port through
//     renderer.renderTargetForInputPort(*in) -- the RenderList-owned
//     intermediate RT. Its texture lands at binding 3+ (line 1911 computes the
//     base for what follows as "3 + m_inputSamplers.size() + ...").
//   * Stage #1 (non-MRT) draws INTO that same intermediate RT through
//     addOutputPass's single-target branch, line 2056-2062
//     ("renderer.renderTargetForOutput(edge)" -> initPass) -- the identical
//     mechanism ISF->ISF chains use.
//   * Stage #2 (MRT) renders through initMRTPass (line 520; all colour
//     textures attached to one render target) and lands attachment k on
//     output-port k's edge through initMRTBlitPasses/initMRTBlitPass
//     (lines 1636/1599) via textureForOutput (line 156).
//
// EXPECTATION, from reading that code: the initial render should be GREEN --
// the from-scratch build path resolves the raster->raster image edge exactly
// like the well-tested ISF->ISF path. Two known sharp edges, either of which
// would turn this red (the motivating score IS crash-named):
//   1. RenderedRawRasterPipelineNode::addInputEdge (line 2285-2300) is a no-op
//      when the UPSTREAM node is a non-MRT raster: textureForOutput returns
//      nullptr when !m_hasMRT (line 158). That only matters on the INCREMENTAL
//      edge path, which this test does not take (all edges exist before
//      create()).
//   2. The resize half: recreateOutputRenderList tears the whole per-output
//      RenderList down and rebuilds it. releaseState resets m_hasMRT
//      (line 2275) and re-init asserts m_inputSamplers is empty (line 1690);
//      a release-ordering bug for CHAINED rasters would surface here, as a
//      crash or a black post-resize readback.
// If this file is red or crashes today, the whole scenario lives in ONE helper
// (run_chain below) so the orchestrator can fork-isolate it or pin the failure
// without restructuring; the assertions state the CORRECT behaviour on
// purpose. Do not weaken them to match a crash.
//
// NEGATIVE CONTROL (product-side, per the spec): in
// src/plugins/score-plugin-gfx/Gfx/Graph/RenderedRawRasterPipelineNode.cpp
// force the second pipeline's MRT off by appending "m_hasMRT = false;" right
// after the assignment at lines 2023-2024. Stage #2 then takes the
// single-target branch (line 2056-2062) for both outgoing edges, attachment 1
// never carries the second FRAGMENT_OUTPUT, and the attachment-1 closed-form
// check below (B == 128, Y ramp) goes red.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_raster_chain_mrt
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_raster_chain_mrt
// QT_QPA_PLATFORM=offscreen must NOT be used: it falls back to the Null
// backend, which produces a stable, self-consistent and completely wrong
// picture.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <array>
#include <cstdlib>
#include <string>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
constexpr int kRampTol = 6;  // 8-bit quantisation of an interpolated ramp
constexpr int kSolidTol = 2; // constant channels

int expected_x(int col, int width)
{
  return int(255.0 * (double(col) + 0.5) / double(width) + 0.5);
}

/// Row 0 is the top of the delivered image and the shader's v_uv.y == 1 is the
/// top, so the green ramp runs 255 at row 0 down to 0 at the last row.
int expected_y_top(int row, int height)
{
  return int(255.0 * (1.0 - (double(row) + 0.5) / double(height)) + 0.5);
}

/// Worst deviation-beyond-tolerance from a closed form over a coarse interior
/// grid. `excess <= 0` means every sampled channel sat within its tolerance.
struct Fit
{
  int excess = -255;
  int col = 0, row = 0;
  char channel = '?';
  int got = 0, expected = 0, tol = 0;

  std::string describe() const
  {
    return "worst channel " + std::string(1, channel) + " at ("
           + std::to_string(col) + "," + std::to_string(row) + "): got "
           + std::to_string(got) + ", expected " + std::to_string(expected)
           + " (tol " + std::to_string(tol) + ")";
  }
};

/// Shared fitter: expected R = X ramp; expected G either the Y ramp (own form)
/// or a constant (upstream sample); constant B and A.
Fit fit_form(const ReadbackImage& img, bool greenIsYRamp, int greenConst,
             int blueConst)
{
  Fit f;
  for(int y = 2; y < img.height - 2; y += 5)
  {
    for(int x = 2; x < img.width - 2; x += 5)
    {
      const auto px = img.at(x, y);
      const int want[4]
          = {expected_x(x, img.width),
             greenIsYRamp ? expected_y_top(y, img.height) : greenConst,
             blueConst, 255};
      const int tol[4]
          = {kRampTol, greenIsYRamp ? kRampTol : kSolidTol, kSolidTol,
             kSolidTol};
      const char names[4] = {'R', 'G', 'B', 'A'};
      for(int c = 0; c < 4; ++c)
      {
        const int excess = std::abs(int(px[c]) - want[c]) - tol[c];
        if(excess > f.excess)
          f = Fit{excess, x, y, names[c], int(px[c]), want[c], tol[c]};
      }
    }
  }
  return f;
}

/// Attachment 0 == the upstream sample: (X ramp, 0.25, 0.75, 1).
/// G = 64 / B = 191 prove the sampler reads stage #1, not black or self.
Fit fit_upstream_sample(const ReadbackImage& img)
{
  return fit_form(img, false, 64, 191);
}

/// Attachment 1 == stage #2's own form: (X ramp, Y ramp top=255, 0.5, 1).
Fit fit_own_ramp(const ReadbackImage& img)
{
  return fit_form(img, true, 0, 128);
}

struct ChainResult
{
  bool skipped = false;
  std::string skip_reason;
  std::string backend;
  std::string error;

  // Initial 64x64 render.
  ReadbackImage a0, a1;
  // After resizing sink0 to 96x64 (sink1 untouched).
  ReadbackImage a0_resized, a1_resized;
};

/// THE WHOLE P0-7 SCENARIO. Kept in one function on purpose so a fork-isolate
/// or crash-pinning wrapper only needs this single entry point.
ChainResult run_chain(score::gfx::GraphicsApi backend)
{
  ChainResult out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    // One viewport-covering triangle per raster stage so both stages shade
    // every pixel (raw-raster draws nothing without geometry).
    const int geo1 = p.addIsf(corpus("syn-geo-producer.cs"));
    const int geo2 = p.addIsf(corpus("syn-geo-producer.cs"));
    const int r1
        = p.addRaster(corpus("syn-rr-chain-src.vs"), corpus("syn-rr-chain-src.fs"));
    const int r2
        = p.addRaster(corpus("syn-rr-chain-mrt.vs"), corpus("syn-rr-chain-mrt.fs"));
    if(geo1 < 0 || geo2 < 0 || r1 < 0 || r2 < 0)
    {
      out.error = p.error().empty() ? "node build failed" : p.error();
      return;
    }

    const int s0 = p.addSink({64, 64});
    const int s1 = p.addSink({64, 64});

    // The load-bearing new capability of this test: a raw-raster IMAGE OUTPUT
    // wired into a second raw-raster's declared image INPUT ("tex"). Name the
    // failure precisely if the port does not exist.
    auto* chainIn = p.imageIn(r2, 0);
    if(!chainIn)
    {
      out.error = "raw-raster #2 exposes no image input port for INPUTS 'tex'";
      return;
    }

    p.wire(p.geometryOut(geo1, 0), p.geometryIn(r1, 0));
    p.wire(p.geometryOut(geo2, 0), p.geometryIn(r2, 0));
    p.wire(p.imageOut(r1, 0), chainIn);          // raster -> raster (the chain)
    p.wire(p.imageOut(r2, 0), p.sinkInput(s0));  // MRT attachment 0
    p.wire(p.imageOut(r2, 1), p.sinkInput(s1));  // MRT attachment 1

    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    p.render(3);
    out.a0 = p.readback(s0);
    out.a1 = p.readback(s1);

    // The graph must SURVIVE a sink resize: BackgroundNode::setSize -> the
    // Graph's onResize -> recreateOutputRenderList (teardown + rebuild of
    // sink0's RenderList at the new size), then render again.
    p.resizeSink(s0, {96, 64});
    p.render(3);
    out.a0_resized = p.readback(s0);
    out.a1_resized = p.readback(s1);

    out.error = p.error();
  });
  return out;
}
} // namespace

TEST_CASE(
    "raw-raster feeding an MRT raw-raster: both attachments correct and the "
    "chain survives a sink resize",
    "[gfx][l3][raster][mrt][chain]")
{
  const auto backend = GENERATE(from_range(platform_backends()));

  const ChainResult r = run_chain(backend);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());

  // ---- Initial render: both attachments valid and closed-form. ----
  // An empty readback on a supported backend FAILS here (valid() checks the
  // byte payload) instead of vacuously passing.
  REQUIRE(r.a0.valid());
  REQUIRE(r.a1.valid());
  CHECK(r.a0.width == 64);
  CHECK(r.a0.height == 64);
  CHECK(r.a1.width == 64);
  CHECK(r.a1.height == 64);

  {
    const Fit f = fit_upstream_sample(r.a0);
    INFO("attachment 0 (upstream sample): " << f.describe());
    CHECK(f.excess <= 0);
  }
  {
    const Fit f = fit_own_ramp(r.a1);
    INFO("attachment 1 (own closed form): " << f.describe());
    CHECK(f.excess <= 0);
  }

  // Belt and braces: the two attachments must be DISTINCT (B = 191 vs 128
  // at the centre) -- one attachment bound twice cannot pass.
  {
    const auto c0 = r.a0.center();
    const auto c1 = r.a1.center();
    INFO("centre B: attachment0=" << int(c0[2]) << " attachment1=" << int(c1[2]));
    CHECK(std::abs(int(c0[2]) - 191) <= kSolidTol);
    CHECK(std::abs(int(c1[2]) - 128) <= kSolidTol);
  }

  // ---- After resizing sink0 to 96x64: both sinks still closed-form. ----
  REQUIRE(r.a0_resized.valid());
  REQUIRE(r.a1_resized.valid());
  CHECK(r.a0_resized.width == 96);
  CHECK(r.a0_resized.height == 64);
  CHECK(r.a1_resized.width == 64);
  CHECK(r.a1_resized.height == 64);

  {
    const Fit f = fit_upstream_sample(r.a0_resized);
    INFO("attachment 0 after resize (96x64): " << f.describe());
    CHECK(f.excess <= 0);
  }
  {
    const Fit f = fit_own_ramp(r.a1_resized);
    INFO("attachment 1 after resize (untouched 64x64): " << f.describe());
    CHECK(f.excess <= 0);
  }
}
