// ISOLATED, EXPECTED-RED finding, in the same spirit as GfxIncrementalFindings /
// GfxVsaCull / CsfImage3d: kept in its own target so the attributable failure
// cannot pull down the green presented-swapchain suites.
//
// FINDING — a live swapchain-flag / -format / graphics-API toggle on an output
// that is part of a Graph frees the QRhi out from under the RenderList that was
// built against it.
//
//   ScreenNode::setSwapchainFlag()  -> destroyOutput()
//   MultiWindowNode::setSwapchainFlag() -> destroyOutput()
//
// destroyOutput() calls RenderState::destroy(), i.e. `delete rhi`. The Graph
// still owns a std::shared_ptr<RenderList> for that output, holding QRhi
// resources (the ScaledRenderer's pipeline, SRB, samplers, UBOs). The next
// touch of that RenderList — Graph::createAllRenderLists, destroyOutputRenderList
// or simply ~Graph, all of which start with `renderer->release()` — dereferences
// the freed QRhi:
//
//   score::gfx::Pipeline::release()            Utils.hpp:139
//   Gfx::ScaledRenderer::release()             InvertYRenderer.cpp:252
//   score::gfx::RenderList::release()          RenderList.cpp:428
//   score::gfx::Graph::~Graph()                Graph.cpp:1112
//   -> QRhiResource::deleteLater()  SIGSEGV
//
// The safe order already exists in the codebase: Graph::destroyOutputRenderList()
// releases the RenderList *then* calls destroyOutput(), and
// Graph::createAllRenderLists() releases every renderer before initializeOutput().
// The three OutputNode setters bypass both and tear the device down directly.
//
// Not reachable from the app *today*: createScreenNode / createMultiWindowNode
// only call these setters at construction time, before the node has ever been
// put in a Graph — but the comments at both call sites document them as live
// toggles ("Live flag change (sRGB toggle) requires the swapchain to be
// recreated ... the Graph reconciler rebuilds them on next cycle"), and wiring
// the sRGB/HDR device parameters to them is the obvious next step.
//
// The guard below is written as the invariant that SHOULD hold — an output that
// destroyed its GPU device must not leave a RenderList behind referencing it —
// so it turns GREEN the day the setters route through destroyOutputRenderList()
// (or release the render list themselves). It deliberately LEAKS the rig: the
// crash is in the teardown, and a crashed process flushes no coverage counters
// and reports no result.

#include "WindowedOutputCommon.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;

TEST_CASE(
    "FINDING: a live swapchain-flag toggle strands the render list",
    "[gfx][window][screen][!mayfail]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  bool skipped{};
  std::string skipReason, backend, error;
  bool stateGone{};
  int renderListsLeft{-1};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    // Leaked on purpose — see the header comment.
    auto* rig = new ScreenRig;
    if(!rig->build(api, {192, 144}))
    {
      skipped = rig->skipped();
      skipReason = rig->skipReason();
      error = rig->error();
      backend = rig->backend();
      if(!skipped)
        delete rig;
      return;
    }
    backend = rig->backend();
    rig->render(2);

    rig->screen->setSwapchainFlag(Gfx::SwapchainFlag::sRGB);

    stateGone = !rig->screen->renderState();
    renderListsLeft = int(rig->graph.renderLists().size());
  });

  if(skipped)
    SKIP(backend << ": " << skipReason);
  REQUIRE(error.empty());

  INFO("backend: " << backend);
  // The device really is gone...
  CHECK(stateGone);
  // ...and this is the defect: a RenderList built against it survives in the
  // Graph and will be released against a freed QRhi.
  CHECK(renderListsLeft == 0);
}
