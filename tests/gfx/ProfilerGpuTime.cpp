// The GPU time a render list records for its frame is in milliseconds:
// QRhiCommandBuffer::lastCompletedGpuTime() gives seconds.

#include "MultiWindowTestCommon.hpp"

#include <Gfx/Graph/GpuTiming.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <QElapsedTimer>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;
using namespace score::test::gfx::multiwindow;

TEST_CASE("the render list's frame GPU time is in milliseconds", "[gfx][profiler]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  auto m = single();
  m.windowSize = {256, 128};
  double gpuMs{-1.}, wallMs{};
  bool timestamps{};
  auto o = runRig(api, m, [&](MultiWindowRig& rig, Outcome&) {
    auto* rl = rig.node->renderer();
    REQUIRE(rl);
    timestamps = rl->state.caps.timestamps;
    rig.render(5);
    constexpr int frames = 20;
    QElapsedTimer t;
    t.start();
    rig.render(frames);
    wallMs = t.nsecsElapsed() / 1e6 / frames;
    for(const auto& e : rl->gpuTimings().snapshot())
      if(e.name == "frame")
        gpuMs = e.last_ms;
  });
  if(o.skipped)
    SKIP(o.backend << ": " << o.skipReason);
  REQUIRE(o.error.empty());
  if(!timestamps || gpuMs <= 0.)
    SKIP(o.backend << ": no GPU timestamps");
  INFO("backend: " << o.backend << ", GPU " << gpuMs << " ms, wall " << wallMs
                   << " ms per frame");
  CHECK(gpuMs > 0.01 * wallMs);
  CHECK(gpuMs < 2. * wallMs + 1.);
}
