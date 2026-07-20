// L3 regression guard — split/threedim finding #3 (cubemap face size never
// clamped to TextureSizeMax + unchecked create() -> render into a dead cube
// texture).
//
// createCubemapTexture() took faceSize straight from the Resolution spinbox
// (up to 8192) with NO clamp to the device TextureSizeMax and ignored
// m_cubemapTex->create(). On a GPU whose TextureSizeMax < the requested size
// (or on low VRAM), create() fails and the code proceeds to build face render
// targets and render into a texture with no valid backing -> validation abort /
// driver crash / silent black. The fix clamps faceSize to
// rhi.resourceLimit(QRhi::TextureSizeMax) and checks create().
//
// createCubemapTexture() needs only a QRhi (no RenderList / command buffer), so
// we drive it directly with a face size FAR above the device limit and assert
// the stored m_faceSize was clamped to the device maximum. Pre-fix, m_faceSize
// keeps the oversized value (RED). We read the return value neither way (its
// signature changed void->bool with the fix) so this TU compiles against BOTH
// the fixed and reverted engine — only the runtime clamp assertion flips.
//
// GPU test — needs DISPLAY; SKIPs where no real RHI device is available.

#include <score_test/App.hpp>
#include <score_test/Gfx.hpp>

// Pre-include CubemapLoader's dependencies with NORMAL access so `#define
// private public` only opens up CubemapLoader's own class body (its include
// guards make the re-includes below no-ops). createCubemapTexture() is private;
// this is the standard test-only access trick, and it does not change layout,
// so the createCubemapTexture symbol from the separately-compiled
// CubemapLoader.cpp (normal access) links fine.
#include <Gfx/Graph/RenderList.hpp>
#include <ossia/dataflow/geometry_port.hpp>
#include <QtGui/private/qrhi_p.h>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>
#include <QDebug>
#include <QImage>

#define private public
#include <Threedim/CubemapLoader.hpp>
#undef private

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string backend;
  bool ran = false;
  int maxSz = 0;
  int requested = 0;
  int faceSize = 0;
};
} // namespace

TEST_CASE(
    "CubemapLoader clamps the cube face size to the device TextureSizeMax",
    "[threedim][cubemap][f3]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Outcome out;
  out.backend = backend_name(backend);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(backend, probed))
    {
      out.skipped = true;
      out.skip_reason = "backend unavailable headless";
      return;
    }

    auto st = score::gfx::createRenderState(backend, QSize{32, 32}, nullptr);
    if(!st || !st->rhi)
    {
      out.skipped = true;
      out.skip_reason = "no QRhi";
      return;
    }
    QRhi& rhi = *st->rhi;

    const int maxSz = rhi.resourceLimit(QRhi::TextureSizeMax);
    if(maxSz <= 0)
    {
      out.skipped = true;
      out.skip_reason = "device reports no TextureSizeMax";
      st->destroy();
      return;
    }

    // A face size WELL above the hard device limit — this is what an 8192
    // (or larger) Resolution turns into on a GPU whose TextureSizeMax is
    // smaller. create() of an unclamped cube of this size would fail.
    const int requested = maxSz + 4096;

    Threedim::CubemapLoader node;
    // Return value intentionally discarded (see file header): valid whether
    // the engine declares this void (pre-fix) or bool (post-fix).
    node.createCubemapTexture(rhi, requested);

    out.maxSz = maxSz;
    out.requested = requested;
    out.faceSize = node.m_faceSize;
    out.ran = true;

    node.releaseCubemapTexture();
    st->destroy();
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO(
      "backend=" << out.backend << " max=" << out.maxSz
                 << " requested=" << out.requested << " face=" << out.faceSize);
  REQUIRE(out.ran);

  // The fix: an over-limit request is clamped to the device maximum, so the
  // cube texture is created at a size the GPU can actually back. Pre-fix,
  // m_faceSize keeps the oversized value (RED).
  CHECK(out.faceSize <= out.maxSz);
  CHECK(out.faceSize == out.maxSz);
}
