// Render-target cases recovered from the 2026-09 graphics review, section 7.
//
// TRIAGE NOTE. The review disowned part of its own evidence here ("initial
// reproduction invalidated ... a sampler/LOD confound", "not compiled or
// executed before the stop request", "not all accompanied by completed
// correction controls"). A case failing was therefore never on its own
// evidence of a defect, so each one below is stated as a contract the engine
// either meets or does not, and every red it produced was chased to a named
// mechanism before anything was changed.
//
// All seven pass. Six needed no argument (01 was already fixed by d6fce67be7,
// 06 was the review's own fixture bug -- see its banner); the other five are
// green because of the engine fixes that ship with this file:
//
//   02  RenderedRawRasterPipelineNode.cpp  m_hasMRT gains an EXECUTION_MODEL term
//   03  SimpleRenderedISFNode.cpp          blit picks its sampler by texture kind
//   04  SimpleRenderedISFNode.cpp          initMRTPass honours OUTPUTS WIDTH/HEIGHT
//   05  RenderedISFNode.cpp                persistent final copy gets the GL Y guard
//   07  SimpleRenderedISFNode.cpp          depth-only RT existence test
//
// Nothing here is pinned, so there is no row for this file in
// cmake/ScoreExpectedRedGuard.cmake.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFileInfo>

#include <cmath>

using namespace score::test::gfx;
namespace
{
QString path(const char* name)
{
  return QString{GFX_TEST_CORPUS_DIR "/RenderTargets-"} + QString::fromUtf8(name);
}
struct Shot
{
  bool skipped{};
  std::string error, reason;
  ReadbackImage image;
  QSize textureSize;
  bool mipmapped{};
  int qtMipLevels{};
};
// Catch2 stringifies std::array<uint8_t,4> as characters, which is unreadable
// in a channel report. Widen to int.
std::array<int, 4> ints(std::array<uint8_t, 4> p)
{
  return {p[0], p[1], p[2], p[3]};
}
void status(Shot& s, GfxPipeline& p)
{
  s.skipped = p.skipped();
  s.reason = p.skipReason();
  s.error = p.error();
}
}

// -----------------------------------------------------------------------------
// 01. The rectangular PER_MIP tail. Fixed in d6fce67be7: the pass count came
// from min(w,h) while Qt allocates from max(w,h), so 128x8 got 8 levels and
// four passes. Read level (count-1) back DIRECTLY -- no sampler in the path,
// which is the confound that made the review withdraw its own version.
//
// The texture must be RE-QUERIED after the first render: frame 1 takes
// mustRecreatePasses and destroys the one create() left behind.
// -----------------------------------------------------------------------------
TEST_CASE(
    "RenderTargets-01 rectangular PER_MIP writes the entire allocated chain",
    "[RenderTargets][mips]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  INFO("backend=" << backend_name(be));
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raw = p.addRaster(path("triangle.vs"), path("mip.fs"));
    const int probe = p.addIsf(path("mip-probe.fs"));
    if(raw < 0 || probe < 0)
    {
      status(s, p);
      return;
    }
    const int sink = p.addSink({128, 8});
    p.wire(p.imageOut(raw), p.imageIn(probe));
    p.wire(p.imageOut(probe), p.sinkInput(sink));
    if(!p.create(be))
    {
      status(s, p);
      return;
    }
    p.render(3);
    status(s, p);
    if(!s.error.empty() || s.skipped)
      return;

    auto* node = p.isf(raw);
    if(node->renderedNodes.empty())
    {
      s.error = "raw renderer absent";
      return;
    }
    // Re-query AFTER the render: the create()-time texture is gone.
    auto [list, renderer] = *node->renderedNodes.begin();
    auto* tex = renderer->textureForOutput(*p.imageOut(raw));
    if(!tex)
    {
      s.error = "raw output absent";
      return;
    }
    s.textureSize = tex->pixelSize();
    s.mipmapped = tex->flags().testFlag(QRhiTexture::MipMapped);
    if(!s.mipmapped)
    {
      s.error = "PER_MIP target has no mip storage";
      return;
    }
    auto& rhi = *list->state.rhi;
    s.qtMipLevels = rhi.mipLevelsForSize(tex->pixelSize());

    QRhiCommandBuffer* cb{};
    if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    {
      s.error = "tail readback frame failed";
      return;
    }
    QRhiReadbackResult rb;
    QRhiReadbackDescription desc(tex);
    desc.setLevel(s.qtMipLevels - 1);
    auto* updates = rhi.nextResourceUpdateBatch();
    updates->readBackTexture(desc, &rb);
    cb->resourceUpdate(updates);
    rhi.endOffscreenFrame();
    rhi.finish();
    s.image.width = rb.pixelSize.width();
    s.image.height = rb.pixelSize.height();
    s.image.bytes = rb.data;
  });
  if(s.skipped)
    SKIP(s.reason);
  INFO(s.error);
  REQUIRE(s.error.empty());
  REQUIRE(s.textureSize == QSize(128, 8));
  REQUIRE(s.qtMipLevels == 8); // floor(log2(128)) + 1
  REQUIRE(s.image.valid());
  const auto px = ints(s.image.center());
  CAPTURE(px);
  // Level 7 is written by PASSINDEX 7 => r = 7/8, b = 1.
  CHECK(std::abs(int(px[0]) - 223) <= 3);
  CHECK(px[2] >= 252);
}

// -----------------------------------------------------------------------------
// 02. R3. Same shader as 01 minus the depth attachment, i.e. ONE colour output.
// The output must still be mipmapped, otherwise a STATIC downstream input
// cannot reach the levels the shader authored.
// -----------------------------------------------------------------------------
TEST_CASE(
    "RenderTargets-02 one-color PER_MIP publishes a mipmapped output",
    "[RenderTargets][mips]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  INFO("backend=" << backend_name(be));
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raw = p.addRaster(path("triangle.vs"), path("single-mip.fs"));
    if(raw < 0)
    {
      status(s, p);
      return;
    }
    const int sink = p.addSink({128, 8});
    p.wire(p.imageOut(raw), p.sinkInput(sink));
    if(!p.create(be))
    {
      status(s, p);
      return;
    }
    p.render(2);
    status(s, p);
    if(!s.error.empty() || s.skipped)
      return;
    auto* n = p.isf(raw);
    if(n->renderedNodes.empty())
    {
      s.error = "raw renderer absent";
      return;
    }
    auto* tex
        = n->renderedNodes.begin()->second->textureForOutput(*p.imageOut(raw));
    s.mipmapped = tex && tex->flags().testFlag(QRhiTexture::MipMapped);
  });
  if(s.skipped)
    SKIP(s.reason);
  INFO(s.error);
  REQUIRE(s.error.empty());
  CHECK(s.mipmapped);
}

// -----------------------------------------------------------------------------
// 03. R1. A LAYERS:2 output rendered through the simple (non-MRT) renderer.
// -----------------------------------------------------------------------------
TEST_CASE(
    "RenderTargets-03 simple array output can be previewed as layer zero",
    "[RenderTargets][array]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  INFO("backend=" << backend_name(be));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(be, {path("array.fs")}, {64, 64}, 3);
  });
  if(r.skipped)
    SKIP(r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  const auto px = ints(r.outputs[0].center());
  CAPTURE(px);
  CHECK(near(r.outputs[0].center(), {64, 128, 191, 255}, 3));
}

// -----------------------------------------------------------------------------
// 04. Explicit per-output WIDTH/HEIGHT on a simple MRT shader.
// -----------------------------------------------------------------------------
TEST_CASE(
    "RenderTargets-04 simple MRT honors explicit output dimensions",
    "[RenderTargets][size]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  INFO("backend=" << backend_name(be));
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int n = p.addIsf(path("sized.fs"));
    if(n < 0)
    {
      status(s, p);
      return;
    }
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(n), p.sinkInput(sink));
    if(!p.create(be))
    {
      status(s, p);
      return;
    }
    p.render(2);
    status(s, p);
    if(!s.error.empty() || s.skipped)
      return;
    if(p.isf(n)->renderedNodes.empty())
    {
      s.error = "renderer absent";
      return;
    }
    auto* t
        = p.isf(n)->renderedNodes.begin()->second->textureForOutput(*p.imageOut(n));
    if(t)
      s.textureSize = t->pixelSize();
  });
  if(s.skipped)
    SKIP(s.reason);
  INFO(s.error);
  REQUIRE(s.error.empty());
  CAPTURE(s.textureSize.width(), s.textureSize.height());
  CHECK(s.textureSize == QSize(17, 9));
}

// -----------------------------------------------------------------------------
// 05. A PERSISTENT single-pass shader must land the same pixels as the same
// shader without the persistent target: the final copy must not flip Y.
// -----------------------------------------------------------------------------
TEST_CASE(
    "RenderTargets-05 persistent final copy preserves image orientation",
    "[RenderTargets][orientation]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  INFO("backend=" << backend_name(be));
  IsfResult a, b;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    a = render_isf_chain(be, {path("direct.fs")}, {64, 64}, 3);
    b = render_isf_chain(be, {path("persistent.fs")}, {64, 64}, 3);
  });
  if(a.skipped || b.skipped)
    SKIP(a.skip_reason + b.skip_reason);
  INFO(a.error);
  INFO(b.error);
  REQUIRE(a.error.empty());
  REQUIRE(b.error.empty());
  REQUIRE(a.outputs.size() == 1);
  REQUIRE(b.outputs.size() == 1);
  REQUIRE(a.outputs[0].valid());
  REQUIRE(b.outputs[0].valid());
  for(int y : {8, 24, 40, 56})
  {
    const auto pa = a.outputs[0].at(16, y), pb = b.outputs[0].at(16, y);
    const auto ia = ints(pa), ib = ints(pb);
    CAPTURE(y, ia, ib);
    CHECK(near(pa, pb, 3));
  }
}

// -----------------------------------------------------------------------------
// 06. FIXTURE BUG in the review's version, not a defect.
//
// The review asserted that a later pass sees the persistent producer's CURRENT
// frame, and rendered ONE frame. That is the opposite of the contract the
// engine documents and implements: RenderedISFNode.cpp:85 -- "Persistent
// texture means that frame N can access the output of this pass at frame N-1"
// -- and createPass() builds the ping-pong that makes it so. Frame 1 therefore
// legitimately reads the never-written half of the pair.
//
// Measured here, both backends: frames=1 -> {0,0,0,255}, frames>=2 -> red. So
// the case is kept, with the contract it actually pins: exactly ONE frame of
// latency, no more. Both directions are asserted, so losing the ping-pong
// (frame 1 red) and losing the persistence (frame 4 black) are both caught.
// -----------------------------------------------------------------------------
TEST_CASE(
    "RenderTargets-06 a later pass reads the persistent producer with one frame of latency",
    "[RenderTargets][persistent]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  INFO("backend=" << backend_name(be));
  const int frames = GENERATE(1, 2, 3, 4);
  INFO("frames=" << frames);
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(be, {path("persistent-current.fs")}, {64, 64}, frames);
  });
  if(r.skipped)
    SKIP(r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  CAPTURE(ints(r.outputs[0].center()));
  if(frames == 1)
    CHECK(near(r.outputs[0].center(), {0, 0, 0, 255}, 3));
  else
    CHECK(near(r.outputs[0].center(), {255, 0, 0, 255}, 3));
}

// -----------------------------------------------------------------------------
// 07. R2. A depth-only source feeding two inputs of one consumer, on the same
// render list. Both samples must read the authored .75 depth.
// -----------------------------------------------------------------------------
TEST_CASE(
    "RenderTargets-07 depth-only source remains shared across same-list edges",
    "[RenderTargets][depth]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  INFO("backend=" << backend_name(be));
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int depth = p.addIsf(path("depth.fs"));
    const int join = p.addIsf(path("depth-join.fs"));
    if(depth < 0 || join < 0)
    {
      status(s, p);
      return;
    }
    const int sink = p.addSink();
    p.wire(p.imageOut(depth), p.imageIn(join, 0));
    p.wire(p.imageOut(depth), p.imageIn(join, 1));
    p.wire(p.imageOut(join), p.sinkInput(sink));
    if(!p.create(be))
    {
      status(s, p);
      return;
    }
    p.render(3);
    status(s, p);
    s.image = p.readback(sink);
  });
  if(s.skipped)
    SKIP(s.reason);
  INFO(s.error);
  REQUIRE(s.error.empty());
  REQUIRE(s.image.valid());
  const auto px = ints(s.image.center());
  CAPTURE(px);
  CHECK(std::abs(int(px[0]) - 191) <= 3);
  CHECK(std::abs(int(px[1]) - 191) <= 3);
}
