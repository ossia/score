// =============================================================================
// L3 INCREMENTAL — runtime render-target FORMAT change on an INTERMEDIATE node
// must rebuild the input render target + passes (spec case P1-13).
//
// Companion of GfxRtChanged.cpp, which changes the SIZE of an intermediate
// node's input render-target spec; this file changes the FORMAT
// (ossia::render_target_spec::format RGBA8 -> RGBA16F) mid-render and proves
// the format actually changed with a closed-form >1.0-survival oracle.
// NO GOLDEN — deliberately: this project once shipped an rgba16f golden that
// asserted the very defect its shader detects, so the oracle here is analytic.
//
// Graph: A (syn-hdr-writer: constant vec4(2.0, 0.5, 0.0, 1.0))
//     -> B (syn-hdr-halver: samples input, outputs rgb * 0.5, alpha 1)
//     -> sink (offscreen RGBA8, 64x64).
//
// The spec on B's image input pins the render target A draws into and B
// samples (the INTERMEDIATE texture). Message path, all public API:
// setRenderTargetSpec (tests/fixtures/score_test/Gfx.hpp:492) calls
// Node::process(int32_t, render_target_spec), which stores the spec in
// Node::renderTargetSpecs and bumps renderTargetChange() (Node.hpp:113-125).
// On the next RenderList::render(), checkForChanges() raises
// renderer->renderTargetSpecsChanged (NodeRenderer.hpp:118), rt_changed is
// accumulated (RenderList.cpp:1058), and — because no full rebuild is pending —
// the SURGICAL branch runs (RenderList.cpp:1081 `if(rt_changed && !rebuilt)`):
//   * phase A detects the change by comparing the live texture against the
//     re-resolved spec, format included:
//       `specChanged = (oldTex->format() != newSpec.format) || ...`
//     (RenderList.cpp:1147), releases the old input RT (RenderList.cpp:1168)
//     and recreates it at the new format (RenderList.cpp:1175
//     `createRenderTarget(state, newSpec.format, newSpec.size, ...)`);
//   * phase B re-inits B's renderer (releaseState/initState,
//     RenderList.cpp:1199-1200) and rebuilds B's OWN output passes so the
//     downstream B->sink pass survives (RenderList.cpp:1219);
//   * phase C re-adds A's upstream pass into the recreated RT
//     (RenderList.cpp:1228).
// We deliberately send a FORMAT-ONLY spec (size left unset):
// Node::resolveRenderTargetSpecs (Node.cpp:418-439) then falls back to
// renderer.state.renderSize (Node.cpp:435-436) = 64x64 = the old size, so the
// ONLY thing that changes is the format — this test cannot pass by riding the
// size-change path GfxRtChanged.cpp already covers.
//
// CLOSED-FORM ORACLE (all values exact in half floats, final readback RGBA8):
//   before (intermediate RGBA8):   A's red 2.0 clamps to 1.0 on UNorm store;
//                                  B outputs 0.5      -> sink red ~128.
//   after  (intermediate RGBA16F): A's red 2.0 survives the float store;
//                                  B outputs 1.0      -> sink red 255.
//   green control channel:         0.5 -> 0.25 -> ~64 in BOTH formats, proving
//                                  the B->sink pass survived and still samples
//                                  A (a dead/black pass cannot fake it).
// Same shaders, two formats — 128 vs 255 on red is the proof the format
// changed; ~64 on green is the proof the passes survived.
//
// NULL FALLBACK (spec P1-13 "Hardware"): with SCORE_TEST_API=null there are no
// meaningful pixels, so only the format-selection decision is asserted:
// Node::resolveRenderTargetSpecs must map the ossia spec to
// QRhiTexture::RGBA8 before / QRhiTexture::RGBA16F after (Node.cpp:427
// `spec.format = ossia_format_to_rhi(it->second.format)`). That decision is
// also asserted on every real backend, as a prelude to the pixel half.
// Backends without RGBA16F render-target support SKIP the pixel half
// (QRhi::isTextureFormatSupported probe).
//
// NEGATIVE CONTROL (product-side, do not commit): neutralize the accumulation
// at src/plugins/score-plugin-gfx/Gfx/Graph/RenderList.cpp:1058
// (`rt_changed |= renderer->renderTargetSpecsChanged;` -> `rt_changed |= false;`).
// The surgical branch at RenderList.cpp:1081 then never runs, the intermediate
// RT stays RGBA8, the >1.0 value clips in both phases, and the post-change
// `solid(out.b, {255, 64, 0, 255})` check goes red (red stays ~128).
// Equivalently, dropping the format term from the comparison at
// RenderList.cpp:1147 fails the same check.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_rt_format_changed
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_rt_format_changed
// =============================================================================
#include "GfxIncrementalCommon.hpp"

#include <Gfx/Graph/RenderList.hpp>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

TEST_CASE(
    "rt format change at runtime rebuilds the passes and preserves >1.0",
    "[gfx][l3][incremental][rtchanged][rtformat]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  int specPort = -1;
  // Node::resolveRenderTargetSpecs' decision (a QRhiTexture::Format), before
  // and after the spec message — asserted on every backend, and the ONLY
  // assertion on the Null backend (spec P1-13 Null fallback).
  int resolvedBefore = -1, resolvedAfter = -1;
  bool fmt16fSupported = false;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("syn-hdr-writer.fs")); // red = 2.0
    const int b = p.addIsf(corpus("syn-hdr-halver.fs")); // rgb * 0.5
    const int s0 = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.imageIn(b, 0)); // A -> B (the spec'd RT)
    p.wire(p.imageOut(b, 0), p.sinkInput(s0)); // B -> sink (must survive)

    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    // RGBA16F render-target support probe for the pixel half's SKIP.
    if(auto rs = p.sink(s0)->renderState(); rs && rs->rhi)
      fmt16fSupported = rs->rhi->isTextureFormatSupported(QRhiTexture::RGBA16F);

    specPort = first_image_input(*p.isf(b));

    // One sink => exactly one RenderList (Graph::renderLists(), Graph.hpp:141).
    const auto& rls = p.graph().renderLists();
    if(rls.empty() || !rls.front())
    {
      out.error = "no render list was created";
      return;
    }
    score::gfx::RenderList& rl = *rls.front();

    // Decision before: no spec stored => default RGBA8 (Node.cpp:418-439,
    // RenderTargetSpecs default at Node.hpp:59).
    resolvedBefore = int(p.isf(b)->resolveRenderTargetSpecs(specPort, rl).format);

    p.render(3);
    out.a = p.readback(s0); // intermediate RGBA8: 2.0 clipped -> red ~128

    // FORMAT-ONLY runtime change of B's input render-target spec: the RT A
    // draws into and B samples becomes RGBA16F; size deliberately unset so it
    // resolves back to the same 64x64 (Node.cpp:435-436) — no size change.
    ossia::render_target_spec spec;
    spec.format = ossia::texture_format::RGBA16F;
    setRenderTargetSpec(*p.isf(b), specPort, spec);

    // Decision after: the spec maps through ossia_format_to_rhi (Node.cpp:427).
    resolvedAfter = int(p.isf(b)->resolveRenderTargetSpecs(specPort, rl).format);

    p.render(3);
    out.b = p.readback(s0); // intermediate RGBA16F: 2.0 survives -> red 255
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO(
      "backend=" << out.backend << " specPort=" << specPort
                 << " resolvedBefore=" << resolvedBefore
                 << " resolvedAfter=" << resolvedAfter
                 << " rgba16f_supported=" << fmt16fSupported);
  REQUIRE(out.error.empty());

  // Format-selection decision (Node::resolveRenderTargetSpecs, Node.cpp:418).
  // Asserted on EVERY backend; the whole assertion set on Null.
  CHECK(resolvedBefore == int(QRhiTexture::RGBA8));
  CHECK(resolvedAfter == int(QRhiTexture::RGBA16F));

  if(backend == score::gfx::Null)
  {
    SUCCEED("Null backend: format-selection decision asserted; no pixels");
    return;
  }

  if(!fmt16fSupported)
    SKIP(out.backend + ": RGBA16F render targets not supported");

  // Before the change (intermediate RGBA8): the produced 2.0 clipped to 1.0,
  // halved to 0.5 -> red ~128; green 0.5 -> 0.25 -> ~64. An empty readback on
  // a supported backend is a FAILURE, not a skip.
  REQUIRE(out.a.valid());
  CHECK(solid(out.a, {128, 64, 0, 255}, 3));

  // After the runtime FORMAT change (intermediate RGBA16F): the 2.0 survived
  // the float round trip, halved to exactly 1.0 -> red 255. Green stays ~64:
  // the recreated input RT is being drawn by A and B's rebuilt B->sink pass is
  // alive (a stale/black texture would zero every channel).
  REQUIRE(out.b.valid());
  CHECK(out.b.width == 64);
  CHECK(out.b.height == 64);
  CHECK(solid(out.b, {255, 64, 0, 255}, 3));

  // Redundant with the solid() above but states the oracle explicitly: the
  // post-change red must exceed anything an RGBA8 intermediate can produce
  // (0.5 * 255 ~= 128) by a wide margin — the >1.0 survival itself.
  CHECK(int(out.b.at(32, 32)[0]) >= 240);
}
