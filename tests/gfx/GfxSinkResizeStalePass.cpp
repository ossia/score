// =============================================================================
// A4 — an incremental edge add that lands between a fast-path sink resize and
// the rebuild that resize defers must not build its pass against the render
// target the resize already destroyed.
//
// THE DEFECT (reproduced 3/3 with
//   score_corpus_tester --seconds 5 \
//     "$SCORE_CORPUS_DIR/2026/scores-2026-02-05/test-js-ui.score"
// — the document whose JS Score.TextureOutlet carries Address "Window:/" while
// the document declares no Window device, so score opens an offscreen
// BackgroundNode output for it and then pushes a render size onto it):
//
//   BackgroundNode::resize()      deleteLater()s m_renderTarget AND
//                                 m_renderState->renderPassDescriptor (outside
//                                 a frame, QRhi deletes both immediately),
//                                 installs fresh ones, then fires onResize.
//   Graph::initializeOutput's     takes the FAST path
//     onResize lambda             RenderList::resizeSwapchainSizedTargets: it
//                                 only records the new sizes and sets
//                                 m_built = false, deferring the renderers'
//                                 release()+init() to the NEXT render frame.
//   -> in that window the output renderer (InvertYRenderer) still holds
//      m_inputTarget, the snapshot it took before the resize, whose
//      QRhiRenderPassDescriptor has already been freed. init() is where it
//      re-adopts the node's live target, and init() has not run yet.
//   GfxContext::incrementalEdgeUpdate      (a cable connected while playing)
//     -> Graph::createAllMissingPasses
//       -> GenericNodeRenderer::addOutputPass
//         -> RenderList::renderTargetForOutput
//           -> InvertYRenderer::renderTargetForInput   -> the stale snapshot
//         -> rt.renderPass->serializedFormat()   SIGSEGV: a virtual call
//                                                through a freed vtable.
//
// The user action is "edit the graph while it runs", so this is a live crash,
// not a shutdown-order artefact.
//
// WHAT IS PINNED HERE, and why it is not the crash itself: whether the freed
// block has been recycled by the time addOutputPass reads it is allocator luck.
// It is recycled reliably in the full application (3/3 SIGSEGV) but in a small
// fixture the replacement render-pass descriptor usually lands on the very
// address the freed one occupied, so the dangling read silently reads the right
// data — and the next render frame's deferred rebuild repairs the pipelines
// anyway, so the PIXELS come out right either way. Neither the crash nor the
// pointer value is therefore a dependable pin.
//
// What IS deterministic is the rule that makes the read impossible: while a
// render list is pending its deferred rebuild it is INCOHERENT with its
// output's GPU objects, so no pass may be built into it. Case 1 asserts exactly
// that — the edge gets no pass at connect time, and gets one on the next render
// frame, once the rebuild has re-adopted the sink's live handles — and then
// checks that the whole corpus sequence (resize, connect, render) does land the
// producer's output at the NEW size.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_sink_resize_stale_pass
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_sink_resize_stale_pass
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

namespace
{
/// Observations collected inside the app lambda (Catch2 macros must run after
/// run_in_gui_app teardown, per the fixture header).
struct PassProbe
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  bool have = false;
  bool pass_right_after_connect = false; // must be false: the list is stale
  bool pass_after_next_frame = false;    // must be true: the rebuild built it
  ReadbackImage c;
};

/// The single Edge on `out`, or nullptr.
inline score::gfx::Edge* sole_edge(score::gfx::Port* out)
{
  return (out && out->edges.size() == 1) ? out->edges.front() : nullptr;
}

/// Does the producer's renderer in the sink's render list already own an output
/// pass for `e`? Re-fetched from renderedNodes every time, because a rebuild
/// replaces the renderer objects.
inline bool has_pass(score::gfx::Node* producer, score::gfx::Edge* e)
{
  if(!producer || !e || producer->renderedNodes.size() != 1)
    return false;
  auto* r = producer->renderedNodes.begin()->second;
  return r && r->hasOutputPassForEdge(*e);
}
} // namespace

TEST_CASE(
    "an edge connected into a just-resized sink gets no pass until the "
    "deferred rebuild has run",
    "[gfx][l3][incremental][resize]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  PassProbe out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    // Producer registered but unwired: the sink's render list comes up with its
    // own output renderer alone, as in the corpus document where the texture
    // outlet addresses a device the document does not declare.
    const int a = p.addIsf(corpus("isf-solid-color.fs"));
    const int s0 = p.addSink({64, 64});

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

    // Fast-path resize: BackgroundNode::resize() frees the render target and the
    // render-pass descriptor and installs fresh ones;
    // RenderList::resizeSwapchainSizedTargets only marks the list not-built and
    // defers the renderers' release()+init() to the next render frame.
    p.resizeSink(s0, {96, 48});

    // NO render() between the resize and the edit — that is the whole point:
    // the rebuild has not run, so the output renderer still holds the
    // pre-resize handles. This is the corpus document's crash site.
    p.addEdgeIncremental(p.imageOut(a, 0), p.sinkInput(s0));

    auto* edge = sole_edge(p.imageOut(a, 0));
    if(!edge)
    {
      out.error = "the incremental add did not produce exactly one edge";
      return;
    }
    out.pass_right_after_connect = has_pass(p.isf(a), edge);

    // The next frame runs maybeRebuild, which release()+init()s every renderer:
    // the output renderer re-adopts the sink's live target and the producer
    // re-adds its output pass, now against handles that are actually alive.
    p.render(3);
    out.pass_after_next_frame = has_pass(p.isf(a), edge);
    out.c = p.readback(s0);
    out.have = true;
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.have);

  // THE PIN. Pre-fix a pass is built here, against the QRhiRenderPassDescriptor
  // BackgroundNode::resize() has already deleted.
  CHECK_FALSE(out.pass_right_after_connect);

  // ...and nothing is lost by deferring it: the rebuild creates it.
  CHECK(out.pass_after_next_frame);
  REQUIRE(out.c.valid());
  CHECK(out.c.width == 96);
  CHECK(out.c.height == 48);
  CHECK(solid(out.c, {255, 0, 255, 255}, 2));
}
