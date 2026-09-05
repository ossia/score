// =============================================================================
// SPEC P0-9: removing a geometry producer mid-render leaves the consumer alive.
//
// Registration:
//   score_add_gfx_test(geometry_producer_removal GfxGeometryProducerRemoval.cpp)
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_geometry_producer_removal
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_geometry_producer_removal
//
// TOPOLOGY (the spec's "A -> geometry filter -> raster -> sink", geometry edges):
//   A  = syn-geo-producer.cs        CSF geometry producer (fullscreen triangle,
//                                   solid green vertex colour — see the shader
//                                   and SyntheticFeatures.cpp:340-356 which pins
//                                   the green readback for this exact chain)
//   F  = syn-passthrough-filter.cs  CSF geometry filter (geometry in + out; used
//                                   as a mid geometry stage by
//                                   SyntheticFeatures.cpp:199)
//   R  = syn-raster-single.{vs,fs}  RAW_RASTER_PIPELINE consumer
//   S  = BackgroundNode offscreen readback sink
//
// LEVEL DRIVEN — Graph, not GfxContext, and why:
//   This file follows tests/gfx/GfxNodeRemoval.cpp, the proven removal pattern.
//   That fixture (tests/fixtures/score_test/Gfx.hpp) drives score::gfx::Graph
//   directly: GfxPipeline::removeNodeIncremental ==
//   Graph::removeNodeAndEdges (Graph.cpp:678) + Graph::removeNode
//   (Graph.cpp:1155). removeNodeAndEdges is the engine path that
//   GfxContext::remove_node-driven removals ultimately rely on for renderer
//   cleanup: it notifies render lists (onEdgeRemoved), deletes the edges,
//   releases the removed node's own renderers
//   (removeNodeFromRenderLists -> Graph.cpp:666-668) and then
//   reconcileAllRenderLists (Graph.cpp:909) releases every renderer of a node
//   the removal made transitively unreachable and erases its
//   node->renderedNodes entry (the release block: Graph.cpp:944-948
//   flushInitialBatch / releaseState / delete / erase / renderedNodesChanged).
//
//   GfxContext's nursery is NOT in this loop: GfxContext::remove_node parks the
//   removed Node object in a nursery (GfxContext.cpp:724) which run_commands
//   flushes via QTimer::singleShot(100, ...) (GfxContext.cpp:878-882) so the
//   Node OBJECT outlives its graph membership by ~100 ms. Inventing a
//   GfxContext harness here would abandon the proven Graph-level fixture; the
//   fixture instead reproduces the nursery window DETERMINISTICALLY: the
//   removed node object stays alive for the rest of the run (owned by
//   GfxPipeline, still receiving per-frame Messages from render()) while it is
//   out of the graph — exactly the state the nursery keeps a node in until the
//   timer fires. Per the spec we still pump 12 frames (>= 10) after the
//   removal, so any deferred/queued destruction that renders interleave with
//   has fired before the post-removal frames are asserted.
//
// CONTRACT ASSERTED (read from the engine source):
//   (a) No crash across removal + 12 further frames + a second removal +
//       teardown; the pipeline error string stays empty.
//   (b) The consumer's post-removal frames must be UNIFORM and either the
//       clear colour or the last valid geometry's green — never garbage from a
//       freed buffer. The raw-raster pass clears its target to Qt::transparent
//       (RenderedRawRasterPipelineNode.cpp:3148-3151, cb.beginPass(rtForPass,
//       Qt::transparent, ...)), and a mesh whose geometry went away drops the
//       pass until geometry comes back (RenderedRawRasterPipelineNode.cpp
//       :487-494 and :1565-1572) — a dropped pass leaves the last valid frame
//       in the target, a running pass with no draw leaves the clear. Both are
//       legitimate; both are uniform and stable, so the post-removal frames
//       must also be identical to each other. A draw from a freed geometry
//       buffer is ASan's to catch; a garbage frame (non-uniform or off-colour)
//       is this test's to catch.
//   (c) The structural invariant GfxNodeRemoval.cpp pins for image nodes,
//       applied to this geometry chain: renderedNodes.size() == 0 for every
//       node the removal made transitively unreachable (Graph.cpp:944-948),
//       while the still-reachable consumers keep their renderers. Removing the
//       TOP producer A orphans exactly {A} (F stays reachable through its
//       F->R edge); the follow-up removal of F then orphans {F}, mirroring
//       GfxNodeRemoval.cpp's staged removals.
//
// NEGATIVE CONTROL (product-side, for the orchestrator):
//   1. Neuter the unreachable-renderer release in
//      Graph::reconcileAllRenderLists — comment out Graph.cpp:945-947
//      (renderer->releaseState(*rl); delete renderer;
//      node->renderedNodes.erase(rn_it);). A's renderedNodes stays at 1 after
//      the removal -> the CHECK(aRenderersAfterRemoveA == 0) below goes red,
//      and the leaked renderer's GPU buffers dangle for ASan.
//   2. The spec's own control — destroy the node immediately instead of via
//      the deferred path: in GfxContext::remove_node replace the
//      nursery.push_back(std::move(node_it->second)) at GfxContext.cpp:724
//      with node_it->second.reset() (or drop the QTimer::singleShot at
//      GfxContext.cpp:880-882 and clear the nursery synchronously inside
//      run_commands) -> use-after-free on the app-level removal path.
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

namespace
{
// All sampled pixels within `tol` of the first sampled pixel (frame is one
// solid colour, whatever it is). Same coarse grid as incremental::solid.
bool uniform_frame(const ReadbackImage& img, int tol = 6)
{
  if(!img.valid())
    return false;
  const auto first = img.at(2, 2);
  for(int y = 2; y < img.height - 2; y += 6)
    for(int x = 2; x < img.width - 2; x += 6)
      if(!near(img.at(x, y), first, tol))
        return false;
  return true;
}

// The last-valid-geometry colour: syn-raster-single.fs paints
// (PASSINDEX/255, 1, 0, 1) over the fullscreen triangle from
// syn-geo-producer.cs -> green. Bounds match SyntheticFeatures.cpp:352-355.
bool greenish(std::array<uint8_t, 4> px)
{
  return px[1] >= 128 && px[0] <= 32 && px[2] <= 32;
}

bool solid_green(const ReadbackImage& img)
{
  if(!img.valid())
    return false;
  for(int y = 2; y < img.height - 2; y += 6)
    for(int x = 2; x < img.width - 2; x += 6)
      if(!greenish(img.at(x, y)))
        return false;
  return true;
}

// The clear colour: the raster pass clears to Qt::transparent
// (RenderedRawRasterPipelineNode.cpp:3148-3151), i.e. near-black RGB. Alpha is
// left free — the sink's blit of a transparent texture is backend-composited.
bool solid_clear(const ReadbackImage& img)
{
  if(!img.valid())
    return false;
  for(int y = 2; y < img.height - 2; y += 6)
    for(int x = 2; x < img.width - 2; x += 6)
    {
      const auto px = img.at(x, y);
      if(px[0] > 8 || px[1] > 8 || px[2] > 8)
        return false;
    }
  return true;
}
}

TEST_CASE(
    "removing a geometry producer mid-render leaves the consumer alive",
    "[gfx][l3][incremental][removal][raster]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  // Captured inside the app lambda (the RenderList / renderer pointers are
  // freed at teardown, so the structural checks must run there).
  Shot out; // a = pre-removal, b = +6 frames post-removal, c = +12 frames
  ReadbackImage afterFilterRemoval;
  bool ran = false;
  std::size_t aRenderersBefore = 0, fRenderersBefore = 0, rRenderersBefore = 0;
  std::size_t aRenderersAfterRemoveA = 0, fRenderersAfterRemoveA = 0,
              rRenderersAfterRemoveA = 0;
  std::size_t fRenderersAfterRemoveF = 0, rRenderersAfterRemoveF = 0;
  bool survivedFurtherRemoval = false;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    // addIsf dispatches on extension: a .cs file builds a CSF compute node
    // (Gfx.hpp make_csf_node), so A and F land in the same index space as the
    // raster node and removeNodeIncremental can address all three.
    const int a = p.addIsf(corpus("syn-geo-producer.cs"));       // producer
    const int f = p.addIsf(corpus("syn-passthrough-filter.cs")); // geo filter
    const int r
        = p.addRaster(corpus("syn-raster-single.vs"), corpus("syn-raster-single.fs"));
    const int s0 = p.addSink({64, 64});
    p.wire(p.geometryOut(a, 0), p.geometryIn(f, 0)); // A -> F (geometry)
    p.wire(p.geometryOut(f, 0), p.geometryIn(r, 0)); // F -> R (geometry)
    p.wire(p.imageOut(r, 0), p.sinkInput(s0));       // R -> sink (image)

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
    out.a = p.readback(s0); // A's green triangle through F, rasterized by R

    aRenderersBefore = p.isf(a)->renderedNodes.size();
    fRenderersBefore = p.isf(f)->renderedNodes.size();
    rRenderersBefore = p.isf(r)->renderedNodes.size();

    // Remove the geometry PRODUCER A while the render loop is live. This is
    // Graph::removeNodeAndEdges + Graph::removeNode; the reconcile inside it
    // (Graph.cpp:944-948) must release A's renderer and erase its
    // renderedNodes entry. F stays reachable (F->R edge intact) and must keep
    // its renderer; so must R. The A node OBJECT stays alive, owned by the
    // fixture — the deterministic equivalent of GfxContext's nursery window
    // (GfxContext.cpp:724, :878-882).
    p.removeNodeIncremental(a);

    aRenderersAfterRemoveA = p.isf(a)->renderedNodes.size();
    fRenderersAfterRemoveA = p.isf(f)->renderedNodes.size();
    rRenderersAfterRemoveA = p.isf(r)->renderedNodes.size();

    // >= 10 frames after the removal (spec P0-9), split so two independent
    // post-removal frames can be compared for stability.
    p.render(6);
    out.b = p.readback(s0);
    p.render(6);
    out.c = p.readback(s0);

    // Staged follow-up, mirroring GfxNodeRemoval.cpp: remove the now
    // geometry-starved filter F too. R keeps its renderer (still wired to the
    // sink); F's must be released. Keep rendering so a UAF in the
    // post-second-removal window would also fire under ASan.
    p.removeNodeIncremental(f);
    fRenderersAfterRemoveF = p.isf(f)->renderedNodes.size();
    rRenderersAfterRemoveF = p.isf(r)->renderedNodes.size();
    p.render(3);
    afterFilterRemoval = p.readback(s0);

    out.error = p.error();
    survivedFurtherRemoval = true;
    ran = true;
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(why);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty()); // (a) pipeline error string empty
  REQUIRE(ran);

  // Before the removal the chain rendered A's fullscreen green triangle.
  REQUIRE(out.a.valid());
  CHECK(uniform_frame(out.a));
  CHECK(solid_green(out.a));

  // Every stage had a live renderer before the removal — this is what proves
  // the CSF geometry producer/filter actually sat inside the RenderList
  // (graphwalk follows geometry edges), so the ==0 checks below have teeth.
  INFO("A renderers before=" << aRenderersBefore
                             << " after removeA=" << aRenderersAfterRemoveA);
  INFO("F renderers before=" << fRenderersBefore
                             << " after removeA=" << fRenderersAfterRemoveA
                             << " after removeF=" << fRenderersAfterRemoveF);
  INFO("R renderers before=" << rRenderersBefore
                             << " after removeA=" << rRenderersAfterRemoveA
                             << " after removeF=" << rRenderersAfterRemoveF);
  CHECK(aRenderersBefore >= 1);
  CHECK(fRenderersBefore >= 1);
  CHECK(rRenderersBefore >= 1);

  // (c) THE GUARD: removing A makes exactly {A} unreachable; its renderer must
  // be released and its renderedNodes entry erased (Graph.cpp:944-948). The
  // still-reachable consumers keep theirs.
  CHECK(aRenderersAfterRemoveA == 0);
  CHECK(fRenderersAfterRemoveA >= 1);
  CHECK(rRenderersAfterRemoveA >= 1);

  // (b) The consumer stayed alive: a non-empty readback on a supported backend
  // is REQUIRED (an empty one is a real failure, not a soft skip)...
  REQUIRE(out.b.valid());
  REQUIRE(out.c.valid());
  // ...and it shows either the clear colour or the last valid geometry —
  // uniform either way, never garbage from a freed buffer.
  CHECK(uniform_frame(out.b));
  CHECK((solid_clear(out.b) || solid_green(out.b)));
  CHECK(uniform_frame(out.c));
  CHECK((solid_clear(out.c) || solid_green(out.c)));
  // Stable across the post-removal renders: frame 6-after and frame 12-after
  // must be the same picture (both legitimate outcomes are deterministic).
  CHECK(max_channel_diff(out.b, out.c) <= 2);

  // Staged follow-up removal: F released, R still alive and still readable.
  CHECK(fRenderersAfterRemoveF == 0);
  CHECK(rRenderersAfterRemoveF >= 1);
  REQUIRE(afterFilterRemoval.valid());
  CHECK(uniform_frame(afterFilterRemoval));
  CHECK((solid_clear(afterFilterRemoval) || solid_green(afterFilterRemoval)));

  // No crash across both removals, 15 post-removal frames and teardown.
  CHECK(survivedFurtherRemoval);
}
