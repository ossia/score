// Image feedback loops through the render graph.
//
// feedback-step-add.fs outputs its input plus 1/32 grey. Wired into a loop, the
// level at the sink climbs with the frame count; a loop that does not carry
// leaves it flat, and a dropped chain leaves the sink black.
//
// * two nodes, the loop closed by a Delayed cable: the reference, works.
// * two nodes, every cable Immediate: one edge of the cycle is treated as
//   delayed, and a warning names the nodes of the cycle.
// * one node, Delayed or Immediate cable straight back into itself carries
//   its state; the Immediate one warns.
#include "GfxIncrementalCommon.hpp"
#include "GfxLogCapture.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
enum class Topology
{
  DelayedPair,
  ImmediatePair,
  DelayedSelf,
  ImmediateSelf
};

const char* topology_name(Topology t)
{
  switch(t)
  {
    case Topology::DelayedPair:
      return "A->B immediate, B->A delayed";
    case Topology::ImmediatePair:
      return "A->B immediate, B->A immediate";
    case Topology::DelayedSelf:
      return "A->A delayed";
    case Topology::ImmediateSelf:
      return "A->A immediate";
  }
  return "?";
}

struct CycleShot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  int early = -1, late = -1;
  bool warned_about_cycle = false;
};

int center_level(const ReadbackImage& img)
{
  if(!img.valid())
    return -1;
  return int(img.center()[0]);
}

// Fills one texture with full-scale magenta and reads it back, in a frame of
// its own. False when the format is not one the test knows how to fill, or
// when the readback does not return what was uploaded.
bool fill_with_magenta(QRhi& rhi, QRhiTexture& tex)
{
  int bpp = 0;
  QByteArray texel;
  switch(tex.format())
  {
    case QRhiTexture::RGBA8:
    case QRhiTexture::BGRA8:
      texel = QByteArray("\xff\x00\xff\xff", 4);
      bpp = 4;
      break;
    case QRhiTexture::RGBA16F: {
      const uint16_t one = 0x3c00;
      const uint16_t h[4]{one, 0, one, one};
      texel = QByteArray(reinterpret_cast<const char*>(h), sizeof(h));
      bpp = 8;
      break;
    }
    case QRhiTexture::RGBA32F: {
      const float f[4]{1.f, 0.f, 1.f, 1.f};
      texel = QByteArray(reinterpret_cast<const char*>(f), sizeof(f));
      bpp = 16;
      break;
    }
    default:
      return false;
  }
  const QSize sz = tex.pixelSize();
  QByteArray data;
  data.reserve(qsizetype(sz.width()) * sz.height() * bpp);
  for(int i = 0; i < sz.width() * sz.height(); ++i)
    data.append(texel);

  QRhiCommandBuffer* cb{};
  if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    return false;
  auto* res = rhi.nextResourceUpdateBatch();
  QRhiTextureSubresourceUploadDescription desc(data);
  desc.setSourceSize(sz);
  res->uploadTexture(&tex, QRhiTextureUploadEntry(0, 0, desc));
  QRhiReadbackResult rb;
  res->readBackTexture(QRhiReadbackDescription(&tex), &rb);
  cb->resourceUpdate(res);
  rhi.endOffscreenFrame();
  return rb.data.size() >= bpp && rb.data.left(bpp) == texel;
}

// Puts garbage into the target `in` is sampled from, in every render list,
// after the graph is created and before its first frame: whatever the driver
// does with fresh memory, the first frame sees magenta unless the render list
// clears the target before anything reads it. False if nothing was filled.
bool plant_garbage(GfxPipeline& p, const score::gfx::Port* in)
{
  if(!in)
    return false;
  bool planted = false;
  for(auto& rl : p.graph().renderLists())
  {
    if(!rl || !rl->state.rhi)
      continue;
    auto rt = rl->renderTargetForInputPort(*in);
    if(!rt.texture)
      continue;
    if(!fill_with_magenta(*rl->state.rhi, *rt.texture))
      return false;
    planted = true;
  }
  return planted;
}

//! The level at the sink after each of the first `frames` frames
struct FirstFrames
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  std::vector<int> levels;
  bool planted = false;
};

FirstFrames render_first_frames(score::gfx::GraphicsApi be, Topology t, int frames)
{
  FirstFrames r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("feedback-step-add.fs"));
    const bool pair = t == Topology::DelayedPair || t == Topology::ImmediatePair;
    const int b = pair ? p.addIsf(corpus("feedback-step-add.fs")) : a;
    const int sink = p.addSink({32, 32});
    if(a < 0 || b < 0)
    {
      r.error = p.error();
      return;
    }
    const auto back = (t == Topology::DelayedPair || t == Topology::DelayedSelf)
                          ? Process::CableType::DelayedGlutton
                          : Process::CableType::ImmediateGlutton;
    if(pair)
      p.wire(p.imageOut(a, 0), p.imageIn(b, 0));
    p.wire(p.imageOut(b, 0), p.imageIn(a, 0), back);
    p.wire(p.imageOut(b, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();
    // The textures the cycle can read before anything draws into them: the
    // input the loop re-enters through, whichever edge breaks it, or the
    // front of the self-fed port.
    r.planted = plant_garbage(p, p.imageIn(a, 0));
    if(pair)
      r.planted = plant_garbage(p, p.imageIn(b, 0)) && r.planted;
    for(int f = 0; f < frames; ++f)
    {
      p.render(1);
      r.levels.push_back(center_level(p.readback(sink)));
    }
    if(r.error.empty())
      r.error = p.error();
  });
  return r;
}

CycleShot render_cycle(score::gfx::GraphicsApi be, Topology t)
{
  CycleShot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    const int a = p.addIsf(corpus("feedback-step-add.fs"));
    const bool pair = t == Topology::DelayedPair || t == Topology::ImmediatePair;
    const int b = pair ? p.addIsf(corpus("feedback-step-add.fs")) : a;
    const int sink = p.addSink({32, 32});
    if(a < 0 || b < 0)
    {
      r.error = p.error();
      return;
    }
    const auto back = (t == Topology::DelayedPair || t == Topology::DelayedSelf)
                          ? Process::CableType::DelayedGlutton
                          : Process::CableType::ImmediateGlutton;
    if(pair)
      p.wire(p.imageOut(a, 0), p.imageIn(b, 0));
    p.wire(p.imageOut(b, 0), p.imageIn(a, 0), back);
    p.wire(p.imageOut(b, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();

    p.render(2);
    r.early = center_level(p.readback(sink));
    p.render(4);
    r.late = center_level(p.readback(sink));
    if(r.error.empty())
      r.error = p.error();
    r.warned_about_cycle = log.count({u"immediate cables form a cycle"}) > 0;
  });
  return r;
}

}

TEST_CASE("a two-node image feedback loop carries its state", "[gfx][feedback]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const auto topo = GENERATE(Topology::DelayedPair, Topology::ImmediatePair);
  CAPTURE(backend_name(backend), topology_name(topo));

  const CycleShot s = render_cycle(backend, topo);
  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  CAPTURE(s.backend, s.early, s.late);

  // One step is 1/32 of full scale, 8 in 8-bit, and the loop takes two steps
  // per frame. After two frames the sink has seen at least two steps; four
  // more frames add eight more, less two for the frame the sink lags behind.
  CHECK(s.early >= 16);
  CHECK(s.late >= s.early + 48);
  CHECK(s.warned_about_cycle == (topo == Topology::ImmediatePair));
}

// The pass into a self-fed port cannot draw into the texture the node
// samples: the render list draws it into a second target and copies it back
// at the start of the next frame.
TEST_CASE(
    "a self-cable carries its state, and an immediate one is reported",
    "[gfx][feedback]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const auto topo = GENERATE(Topology::DelayedSelf, Topology::ImmediateSelf);
  CAPTURE(backend_name(backend), topology_name(topo));

  const CycleShot s = render_cycle(backend, topo);
  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  CAPTURE(s.backend, s.early, s.late);

  CHECK(s.early >= 16);
  CHECK(s.late >= s.early + 24);
  CHECK(s.warned_about_cycle == (topo == Topology::ImmediateSelf));
}

// The same self-cable added and removed while rendering, through the
// incremental path the app uses (reconcile + createAllMissingPasses).
TEST_CASE("a self-cable added live carries its state", "[gfx][feedback]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  CycleShot s;
  int before = -1, after = -1;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("feedback-step-add.fs"));
    const int sink = p.addSink({32, 32});
    if(a < 0)
    {
      s.error = p.error();
      return;
    }
    p.wire(p.imageOut(a, 0), p.sinkInput(sink));
    if(!p.create(backend))
    {
      s.skipped = p.skipped();
      s.skip_reason = p.skipReason();
      s.error = p.error();
      return;
    }
    s.backend = p.backend();

    p.render(2);
    before = center_level(p.readback(sink));
    p.addEdgeIncremental(
        p.imageOut(a, 0), p.imageIn(a, 0), Process::CableType::DelayedGlutton);
    p.render(2);
    s.early = center_level(p.readback(sink));
    p.render(4);
    s.late = center_level(p.readback(sink));
    p.removeEdgeIncremental(p.imageOut(a, 0), p.imageIn(a, 0));
    p.render(2);
    after = center_level(p.readback(sink));
    if(s.error.empty())
      s.error = p.error();
  });

  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  CAPTURE(s.backend, before, s.early, s.late, after);

  CHECK(before >= 6);
  CHECK(before <= 10);
  CHECK(s.early >= before + 8);
  CHECK(s.late >= s.early + 24);
  CHECK(after >= 6);
  CHECK(after <= 10);
}

// The textures a cycle reads before anything has drawn into them (the input
// of the node the loop re-enters, the front of a self-fed port) start out
// black, so the level is exact from the first frame on: one step of 8 (1/32 of
// 255, rounded) per pass. They are filled with magenta between the creation of
// the graph and its first frame; a target not cleared at creation reads it.
TEST_CASE("a feedback loop starts from black", "[gfx][feedback]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const auto topo = GENERATE(
      Topology::DelayedPair, Topology::ImmediatePair, Topology::DelayedSelf,
      Topology::ImmediateSelf);
  CAPTURE(backend_name(backend), topology_name(topo));

  const FirstFrames s = render_first_frames(backend, topo, 4);
  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  CAPTURE(s.backend, s.levels);
  REQUIRE(s.planted);
  REQUIRE(s.levels.size() == 4);

  // A self-cable: one pass per frame, the first one reading black.
  // A pair: B draws one pass per outgoing cable, each when its consumer's
  // input is prepared. In the first frame B -> A reads B's black input (8),
  // A reads that (16), B -> sink reads A's (24); every later frame adds the
  // two steps of A and B.
  const bool pair = topo == Topology::DelayedPair || topo == Topology::ImmediatePair;
  for(int f = 0; f < 4; ++f)
  {
    CAPTURE(f);
    CHECK(s.levels[f] == (pair ? 8 + 16 * (f + 1) : 8 * (f + 1)));
  }
}

// A self-fed raw raster sampling its own depth reads, on its first frame, the
// depth its input target was created with. It must be the far end of the
// compare the node draws with, as the clears of every later frame: 0 under
// greater (reverse-Z), 1 under less. The near end would read as a scene
// touching the camera.
TEST_CASE("a self-fed depth input starts at the far plane", "[gfx][feedback][depth]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  const bool less = GENERATE(false, true);
  CAPTURE(backend_name(backend), less);

  FirstFrames s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addRaster(
        corpus("selffb-depth.vs"),
        corpus(less ? "selffb-depth-less.fs" : "selffb-depth.fs"));
    const int sink = p.addSink({32, 32});
    if(a < 0)
    {
      s.error = p.error();
      return;
    }
    p.wire(p.imageOut(a, 0), p.imageIn(a, 0), Process::CableType::DelayedGlutton);
    p.wire(p.imageOut(a, 0), p.sinkInput(sink));
    if(!p.create(backend))
    {
      s.skipped = p.skipped();
      s.skip_reason = p.skipReason();
      s.error = p.skipped() ? std::string{} : p.error();
      return;
    }
    s.backend = p.backend();
    for(int f = 0; f < 2; ++f)
    {
      p.render(1);
      s.levels.push_back(center_level(p.readback(sink)));
    }
    if(s.error.empty())
      s.error = p.error();
  });
  if(s.skipped)
    SKIP(s.skip_reason);
  REQUIRE(s.error.empty());
  CAPTURE(s.backend, s.levels);
  REQUIRE(s.levels.size() == 2);

  // Red is the sampled depth: the far plane first, then the 0.5 (128) the node
  // wrote the frame before.
  CHECK(s.levels[0] == (less ? 255 : 0));
  CHECK(s.levels[1] >= 120);
  CHECK(s.levels[1] <= 136);
}
