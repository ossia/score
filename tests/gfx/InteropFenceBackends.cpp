// L3: makeInteropFence() against a REAL QRhi, one case per backend the box
// actually brings up.
//
// This is the primitive that decides whether a peer device may DMA-read a
// buffer the GPU is still writing. Its failure mode is silent and destructive:
// a fence that answers "yes, the GPU is done" when it is not tears a frame on
// the wire, and a fence that answers "no" forever stalls the rung instead. The
// three questions worth a real device are therefore:
//
//   1. Does the factory hand back the implementation for THIS backend?
//   2. Does that implementation refuse to initialise when the machinery it
//      needs is absent, rather than reporting a synchronisation it cannot do?
//   3. Once released, does it go back to refusing?
//
// The Null-backend half of this surface (the fallback stub) is covered without
// a GPU in src/plugins/score-plugin-gfx/tests/InteropRingPolicyTest.cpp; this
// file is the part that needs a device.

#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/interop/InteropFence.hpp>

#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <string>

namespace
{
struct FenceProbe
{
  bool skipped{true};
  std::string skip_reason;
  std::string backend;
  QRhi::Implementation impl{};

  bool nonNull{};
  bool validBeforeInit{};
  bool initWithoutCuda{};
  bool validAfterInit{};
  bool waitAfterInit{};
  bool validAfterRelease{};
  bool waitAfterRelease{};
  bool doubleReleaseSurvived{};
};

FenceProbe probeFence(score::gfx::GraphicsApi api)
{
  FenceProbe out;
  std::string name;
  if(!score::test::gfx::probe_api(api, name))
  {
    out.skip_reason = "backend unavailable on this host";
    out.backend = name;
    return out;
  }
  out.backend = name;

  auto st = score::gfx::createRenderState(api, QSize{16, 16}, nullptr);
  if(!st || !st->rhi)
  {
    out.skip_reason = "createRenderState produced no QRhi";
    return out;
  }
  if(api != score::gfx::Null && st->rhi->backend() == QRhi::Implementation::Null)
  {
    out.skip_reason = "fell back to the Null RHI backend; every verdict here "
                      "would be void";
    st->destroy();
    return out;
  }

  out.impl = st->rhi->backend();

  {
    auto fence = score::gfx::interop::makeInteropFence(*st->rhi);
    out.nonNull = (fence != nullptr);
    if(fence)
    {
      out.validBeforeInit = fence->valid();
      // No CUDA context: the only bridge the D3D12/Vulkan implementations can
      // import their semaphore into.
      out.initWithoutCuda = fence->init(*st->rhi, nullptr);
      out.validAfterInit = fence->valid();
      out.waitAfterInit = fence->waitOnCuda(1);
      fence->release();
      out.validAfterRelease = fence->valid();
      out.waitAfterRelease = fence->waitOnCuda(1);
      fence->release();
      out.doubleReleaseSurvived = true;
    }
  }

  st->destroy();
  out.skipped = false;
  return out;
}
}

TEST_CASE("the interop fence never claims a synchronisation it cannot make",
          "[gfx][interop][fence]")
{
  const auto api = GENERATE(from_range(score::test::gfx::platform_backends()));

  FenceProbe r;
  score::test::run_in_gui_app(
      [&](const score::GUIApplicationContext&) { r = probeFence(api); });

  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);

  INFO("backend: " << r.backend);

  REQUIRE(r.nonNull);
  // Vendor adapters call valid() before anything else and route around a
  // false; a fence that claimed validity before init() would be believed.
  CHECK_FALSE(r.validBeforeInit);
  CHECK(r.doubleReleaseSurvived);

  // valid() and init() must agree in both directions.
  CHECK(r.validAfterInit == r.initWithoutCuda);
  // An implementation that did not initialise must not report a completed
  // wait; that is the answer that lets a peer read a half-written frame.
  if(!r.initWithoutCuda)
    CHECK_FALSE(r.waitAfterInit);

  // release() puts it back to refusing, whatever it was before.
  CHECK_FALSE(r.validAfterRelease);
  CHECK_FALSE(r.waitAfterRelease);

  switch(r.impl)
  {
    case QRhi::OpenGLES2:
      // InteropFenceGL synchronises with glFinish() and needs no CUDA
      // context, so it must come up from the QRhi alone and then answer its
      // wait -- the GL rung would otherwise never engage.
      CHECK(r.initWithoutCuda);
      CHECK(r.waitAfterInit);
      break;
    case QRhi::Vulkan:
      // InteropFenceVulkan exports a VkSemaphore and imports it into CUDA.
      // With no CUDA context there is nothing to import into, so honest
      // behaviour is to decline and let the caller use its finish() fallback.
      CHECK_FALSE(r.initWithoutCuda);
      CHECK_FALSE(r.waitAfterInit);
      break;
    case QRhi::D3D12:
      // Documented stub: blocked on the same QRhi D3D12 SHARED-heap
      // limitation as the buffer ring.
      CHECK_FALSE(r.initWithoutCuda);
      CHECK_FALSE(r.waitAfterInit);
      break;
    case QRhi::D3D11:
      // The immediate-context flush at endOffscreenFrame is the fence, so
      // this one is a no-op that legitimately succeeds.
      CHECK(r.initWithoutCuda);
      CHECK(r.waitAfterInit);
      break;
    default:
      break;
  }
}
