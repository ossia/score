// =============================================================================
// L3 INCREMENTAL -- a runtime render-target-spec change on an intermediate node
// must not drop that node's own output passes.
//
// Graph: A (solid magenta) -> B (plain passthrough) -> sink.
//
// At runtime the user changes B's input render-target spec (resolution) via a
// render_target_spec message -- no restart, no viewport resize. B->renderTarget
// SpecsChanged fires and the surgical rt_changed path in RenderList::render()
// runs instead of a full rebuild. Phase B releaseState()+initState() wipes B's
// own per-edge output pass (B->sink) and its pipeline cache, so those output
// passes are rebuilt right after initState, exactly as init() does.
//
// Regression guard: after the spec change the sink still shows B's magenta
// output. Without that rebuild B's B->sink pass is gone and never restored, the
// sink samples B's stale/black texture forever, and the sink goes black.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_rt_changed
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_rt_changed
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

TEST_CASE(
    "rt_changed on an intermediate node keeps its output pass alive",
    "[gfx][l3][incremental][rtchanged]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  int specPort = -1;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs"));      // magenta source
    const int b = p.addIsf(corpus("isf-passthrough-plain.fs")); // intermediate
    const int s0 = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.imageIn(b, 0)); // A -> B
    p.wire(p.imageOut(b, 0), p.sinkInput(s0)); // B -> sink

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
    out.a = p.readback(s0); // B passes A's magenta through -> magenta

    // Change B's INPUT render-target spec (the RT A draws into and B samples).
    // A different resolution => the surgical rt_changed path recreates that
    // input RT and re-inits B; B's OWN B->sink output pass must be rebuilt.
    specPort = first_image_input(*p.isf(b));
    ossia::render_target_spec spec;
    spec.size = ossia::texture_size{32, 32};
    setRenderTargetSpec(*p.isf(b), specPort, spec);

    p.render(3);
    out.b = p.readback(s0); // still magenta; black means the pass was dropped
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend << " specPort=" << specPort);
  REQUIRE(out.error.empty());

  // Before the change: sink shows B's passthrough of A's magenta.
  REQUIRE(out.a.valid());
  CHECK(solid(out.a, {255, 0, 255, 255}, 3));

  // After the runtime spec change: B must still be producing into the sink.
  REQUIRE(out.b.valid());
  CHECK(out.b.width == 64);
  CHECK(out.b.height == 64);
  CHECK(solid(out.b, {255, 0, 255, 255}, 3));
}
