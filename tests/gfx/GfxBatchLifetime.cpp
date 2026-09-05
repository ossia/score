// =============================================================================
// L3 INCREMENTAL — a pending resource-update batch must not outlive the
// resources it names.
//
// Graph mutations run between frames. An incremental edge addition creates a
// renderer whose initState() queues updateDynamicBuffer(materialUBO, ...) into
// a batch that is merged into the RenderList's initial batch and stays pending
// until the next render frame. If a second mutation lands in the same
// inter-frame window and tears that renderer down (node removal ->
// removeNodeFromRenderLists, or edge removal -> reconcile step 3), the batch
// keeps a raw pointer to the freed QRhiBuffer: QRhiResourceUpdateBatch stores
// resource pointers until commit, and deleteLater() outside a frame deletes
// immediately. The next render() then submits the batch and the backend's
// enqueueResourceUpdates writes through the dangling pointer — the
// OpenGL/D3D11/Vulkan crash triplet re-symbolized from the corpus run
// (RenderList::render -> resourceUpdate / beginPass).
//
// REGRESSION GUARD. Both cases build a rendered A -> sink graph, then, with NO
// render in between: add node B + edge B -> sink incrementally (leaves B's
// material-UBO upload pending in the initial batch), tear B down through the
// same incremental path the app uses, and render again. Pre-fix this is a
// heap-use-after-free inside QRhi's enqueueResourceUpdates on the very next
// frame (hard crash under ASan). Post-fix the pending batch is submitted
// before the teardown and the sink still shows A. GREEN on OpenGL and Vulkan.
// Do NOT weaken.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_batch_lifetime
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_batch_lifetime
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

TEST_CASE(
    "node removed while its init updates are still batched",
    "[gfx][l3][incremental][batchlife]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  bool ran = false;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs")); // magenta source
    const int s0 = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.sinkInput(s0));

    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    p.render(2);
    out.a = p.readback(s0);

    // isf-control-color has a material input, so reconcile's initState queues
    // an updateDynamicBuffer(materialUBO) into the pending initial batch.
    const int b = p.addIsf(corpus("isf-control-color.fs"));
    p.addEdgeIncremental(p.imageOut(b, 0), p.sinkInput(s0));

    // No render in between: the batch is still pending when the removal path
    // deletes B's renderer resources.
    p.removeNodeIncremental(b);

    p.render(2);
    out.b = p.readback(s0);
    ran = true;
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(ran);

  REQUIRE(out.a.valid());
  CHECK(solid(out.a, {255, 0, 255, 255}, 3));

  // Post-mutation frame rendered (pre-fix: UAF crash before reaching here)
  // and still shows A.
  REQUIRE(out.b.valid());
  CHECK(solid(out.b, {255, 0, 255, 255}, 3));
}

TEST_CASE(
    "edge removed while the sink side's init updates are still batched",
    "[gfx][l3][incremental][batchlife]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  bool ran = false;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("isf-solid-color.fs"));
    const int s0 = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.sinkInput(s0));

    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    p.render(2);
    out.a = p.readback(s0);

    // Connect, then disconnect without a frame in between: B becomes
    // unreachable and reconcile step 3 releases its renderer while the
    // initial batch still holds B's queued uploads.
    const int b = p.addIsf(corpus("isf-control-color.fs"));
    p.addEdgeIncremental(p.imageOut(b, 0), p.sinkInput(s0));
    p.removeEdgeIncremental(p.imageOut(b, 0), p.sinkInput(s0));

    p.render(2);
    out.b = p.readback(s0);
    ran = true;
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(ran);

  REQUIRE(out.a.valid());
  CHECK(solid(out.a, {255, 0, 255, 255}, 3));

  REQUIRE(out.b.valid());
  CHECK(solid(out.b, {255, 0, 255, 255}, 3));
}
