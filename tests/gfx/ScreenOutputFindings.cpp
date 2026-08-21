// Two regression guards on the ScreenNode <-> Graph boundary.
//
// (1) A live swapchain-flag or -format toggle must not strand the RenderList.
//     ScreenNode::setSwapchainFlag() and MultiWindowNode::setSwapchainFlag()
//     reach destroyOutput(), which calls RenderState::destroy(), i.e.
//     `delete rhi`, while the Graph still owns a shared_ptr<RenderList> holding
//     QRhi resources (the ScaledRenderer's pipeline, SRB, samplers, UBOs). The
//     order that works already existed in Graph::destroyOutputRenderList():
//     release and deregister the RenderList first, destroy the output second.
//     The setters take that order through
//     OutputConfiguration::onReleaseRenderList, the callback the Graph hands
//     every output it initialises.
//
//     The rig is destroyed normally here on purpose: letting ~ScreenRig run IS
//     the assertion.
//
// (2) A window resize must not discard the render-size override, which decouples
//     the resolution the graph renders at from the window it is presented in
//     (window_device's "/rendersize", and ScreenNode::setRenderSize underneath).
//     Window::resizeSwapChain() -> onResize() -> initializeOutput() reaches
//     RenderList::resizeSwapchainSizedTargets(rs->outputSize), whose fast path
//     must take both sizes from the RenderState the output node has already
//     updated rather than writing the swapchain size straight into
//     RenderState::renderSize.

#include "WindowedOutputCommon.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;

TEST_CASE(
    "a live swapchain-flag toggle releases the render list first",
    "[gfx][window][screen][regression]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  bool skipped{};
  std::string skipReason, backend, error;
  bool stateGone{};
  int renderListsBefore{-1};
  int renderListsLeft{-1};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {192, 144}))
    {
      skipped = rig.skipped();
      skipReason = rig.skipReason();
      error = rig.error();
      backend = rig.backend();
      return;
    }
    backend = rig.backend();
    rig.render(2);

    renderListsBefore = int(rig.graph.renderLists().size());

    rig.screen->setSwapchainFlag(Gfx::SwapchainFlag::sRGB);

    stateGone = !rig.screen->renderState();
    renderListsLeft = int(rig.graph.renderLists().size());

    // ~ScreenRig runs here: ~Graph walks m_renderers and releases each one.
    // Before the fix that walk hit a RenderList whose QRhi had just been
    // deleted.
  });

  if(skipped)
    SKIP(backend << ": " << skipReason);
  REQUIRE(error.empty());

  INFO("backend: " << backend);
  // The rig really did have a render list to strand...
  CHECK(renderListsBefore == 1);
  // ...the device really is gone...
  CHECK(stateGone);
  // ...and no RenderList built against it survives in the Graph.
  CHECK(renderListsLeft == 0);
}

TEST_CASE(
    "a window resize keeps the render-size override",
    "[gfx][window][screen][regression]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  bool skipped{};
  std::string skipReason, backend, error;
  const QSize requested{200, 120};
  QSize beforeResize, afterResize, outputAfterResize;

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    ScreenRig rig;
    if(!rig.build(api, {320, 240}))
    {
      skipped = rig.skipped();
      skipReason = rig.skipReason();
      error = rig.error();
      backend = rig.backend();
      return;
    }
    backend = rig.backend();
    rig.render(3);

    rig.screen->setRenderSize(requested);
    rig.render(2);
    if(auto rs = rig.screen->renderState())
      beforeResize = rs->renderSize;

    // Now resize the window. The override must survive it.
    const auto out = rig.screen->renderState()->outputSize;
    rig.screen->setSize({420, 320});
    pump_until(
        [&] {
      rig.render(1);
      return rig.screen->renderState()->outputSize != out;
        },
        5000);
    rig.render(3);
    if(auto rs = rig.screen->renderState())
    {
      afterResize = rs->renderSize;
      outputAfterResize = rs->outputSize;
    }
  });

  if(skipped)
    SKIP(backend << ": " << skipReason);
  REQUIRE(error.empty());

  INFO("backend: " << backend << " before " << beforeResize.width() << "x"
                   << beforeResize.height() << " after " << afterResize.width() << "x"
                   << afterResize.height() << " output " << outputAfterResize.width()
                   << "x" << outputAfterResize.height());
  // Setting it works...
  CHECK(beforeResize == requested);
  // ...the window really did resize (otherwise the check below is vacuous)...
  CHECK(outputAfterResize != requested);
  // ...and the resize did not throw the override away.
  CHECK(afterResize == requested);
}
