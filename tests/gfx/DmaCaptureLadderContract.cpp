// =============================================================================
// L3 -- the two things DMACaptureInputNode::Renderer::init() must tell its
// backend once the capture ladder has resolved. Both are decisions the backend
// cannot observe from its own side, and both are invisible in a rendered frame:
// they were each found by reading the code, not by looking at a picture.
//
//   1. Whether the sync group it offered actually engaged. The renderer declines
//      a group whenever the rung that engaged cannot bind a caller-chosen slot,
//      and the two paths disagree about who owns a slot -- ungrouped, the
//      strategy's publisher decides when one goes back to the device; grouped,
//      the group decides. A backend that was not told keeps asking the publisher
//      and hands the device back the very frame the group has just bound. That
//      surfaces as a rare tear, which is exactly the failure a green render
//      never catches.
//
//   2. Whether the whole-frame external-image rung engaged. The decoder is
//      chosen BEFORE the ladder runs, so a backend that asked for the external
//      form already holds a decoder only that rung can feed. If the rung
//      declines, that decoder samples a texture nothing ever uploads to: a black
//      frame from a path reporting itself engaged.
//
// The backend, the strategies and the decoder are all fakes, because what is
// under test is init()'s decision sequence rather than any driver. What is real
// is the QRhi, the RenderList and the node itself -- the decisions happen inside
// init(), which only runs against a live render list, so there is no cheaper
// seam than a graph.
//
// SCOPE. The fake decoder is a PackedDecoder either way: "needs the external
// image" is a property the backend reports, not something a stand-in decoder can
// embody, so what is asserted is that the node re-asks for a decoder and drops
// the request -- not that the second decoder samples differently.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/DMACaptureInputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Graph/interop/CaptureSyncGroup.hpp>
#include <Gfx/Graph/interop/VideoCaptureStrategy.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>

using namespace score::test::gfx;

namespace
{
constexpr int kWidth = 64;
constexpr int kHeight = 64;

//! A rung of the ladder. Says only whether it initialises and whether it can
//! bind a slot somebody else chose -- the two properties init() branches on.
struct FakeStrategy final : score::gfx::interop::VideoCaptureStrategy
{
  FakeStrategy(const char* n, bool initOk, bool slotSelection)
      : m_name{n}
      , m_initOk{initOk}
      , m_slotSelection{slotSelection}
  {
  }

  const char* name() const noexcept override { return m_name; }

  bool init(const score::gfx::interop::VideoCaptureStrategyConfig& cfg) override
  {
    // Keep the decoder's own texture rather than supplying one: the zero-copy
    // texture swap is a different contract and would only add noise here.
    m_tex = cfg.outputTexture;
    return m_initOk;
  }
  void release() override { }
  std::size_t slotCount() const noexcept override { return 3; }
  void* slotBuffer(std::size_t) const noexcept override { return nullptr; }
  bool ingestFrame(std::size_t) override { return true; }
  QRhiTexture* outputTexture() const noexcept override { return m_tex; }
  void acquireForRender() override { }
  void releaseAfterRender() override { }
  bool supportsSlotSelection() const noexcept override { return m_slotSelection; }

  const char* m_name{};
  bool m_initOk{};
  bool m_slotSelection{};
  QRhiTexture* m_tex{};
};

//! What the vendor backend under test is set up to do.
struct Plan
{
  //! Whether the GPU-direct rung initialises. False walks the ladder off its end
  //! into the CPU-staging fallback, which is where both defects surface.
  bool zeroCopyRungInits{false};
  //! Whether the rung that ends up engaged can bind a caller-chosen slot.
  bool engagedRungBindsChosenSlot{false};
  //! Whether this backend drives several sensors off one capture.
  score::gfx::interop::CaptureSyncGroup* group{};
  //! Whether makeDecoder() returns a decoder only the external rung can feed.
  bool decoderNeedsExternal{false};
};

//! Everything the node told the backend, read back after init().
struct Told
{
  int decodersMade{0};
  int dropExternalCalls{0};
  int syncEngagedCalls{0};
  bool syncEngaged{false};
  bool stillWantsExternal{false};
  const char* engagedRung{};
};

struct FakeBackend final : score::gfx::DMACaptureBackend
{
  explicit FakeBackend(Plan p)
      : plan{p}
      , m_wantsExternal{p.decoderNeedsExternal}
  {
  }

  bool open() override { return true; }
  int width() const noexcept override { return kWidth; }
  int height() const noexcept override { return kHeight; }
  std::uint32_t frameByteSize() const noexcept override
  {
    return std::uint32_t(kWidth) * kHeight * 4u;
  }

  Video::ImageFormat imageFormat() const override
  {
    Video::ImageFormat f;
    f.width = kWidth;
    f.height = kHeight;
    return f;
  }

  std::unique_ptr<score::gfx::GPUVideoDecoder>
  makeDecoder(Video::VideoMetadata& meta) override
  {
    ++told.decodersMade;
    return std::make_unique<score::gfx::PackedDecoder>(
        QRhiTexture::RGBA8, 4, meta);
  }

  std::unique_ptr<score::gfx::interop::VideoCaptureStrategy> pickStrategy(
      QRhi::Implementation, const score::gfx::interop::GpuCapabilities&) override
  {
    // The whole-frame external image is a property of this rung, so an offered
    // rung that does not initialise is the "the external form declined" case.
    return std::make_unique<FakeStrategy>(
        "fake-zerocopy", plan.zeroCopyRungInits, plan.engagedRungBindsChosenSlot);
  }

  std::unique_ptr<score::gfx::interop::VideoCaptureStrategy>
  makeCpuStrategy() override
  {
    return std::make_unique<FakeStrategy>(
        "fake-cpu-staging", true, plan.engagedRungBindsChosenSlot);
  }

  void setStrategy(score::gfx::interop::VideoCaptureStrategy* s) noexcept override
  {
    told.engagedRung = s ? s->name() : nullptr;
  }
  void start() override { }
  void stop() override { }

  SyncMembership syncGroup() noexcept override { return {plan.group, 0}; }

  void setSyncGroupEngaged(bool engaged) noexcept override
  {
    ++told.syncEngagedCalls;
    told.syncEngaged = engaged;
  }

  bool decoderNeedsExternalImage() const noexcept override
  {
    return m_wantsExternal;
  }
  void dropExternalImageRequest() noexcept override
  {
    ++told.dropExternalCalls;
    m_wantsExternal = false;
  }

  Plan plan;
  Told told;
  bool m_wantsExternal{};
};

struct FakeCaptureNode final : score::gfx::DMACaptureInputNode
{
  explicit FakeCaptureNode(Plan p)
      : plan{p}
  {
  }

  std::unique_ptr<score::gfx::DMACaptureBackend>
  makeCaptureBackend(score::gfx::interop::VideoCaptureSlotRing&) const override
  {
    auto b = std::make_unique<FakeBackend>(plan);
    live = b.get();
    return b;
  }

  Plan plan;
  //! The renderer owns the backend, so this is only valid while the graph is.
  mutable FakeBackend* live{};
};

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  bool ran = false;
  Told told;
};

//! fake capture node -> offscreen sink, brought up on `api`, one frame pumped.
Outcome run(score::gfx::GraphicsApi api, Plan plan)
{
  Outcome out;
  out.backend = backend_name(api);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto node = std::make_unique<FakeCaptureNode>(plan);
    auto* raw = node.get();
    const int cap = p.addNode(std::move(node));
    if(cap < 0)
    {
      out.error = p.error();
      return;
    }
    const int sink = p.addSink({kWidth, kHeight});
    p.wire(p.nodeImageOut(cap, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    p.render(1);

    if(!raw->live)
    {
      out.error = "the renderer never built a capture backend";
      return;
    }
    // Copied out here on purpose: the backend dies with the render list, which
    // the pipeline drops as this scope closes.
    out.told = raw->live->told;
    out.told.stillWantsExternal = raw->live->m_wantsExternal;
    out.ran = true;
  });
  return out;
}

void requireRan(const Outcome& o)
{
  if(o.skipped)
    SKIP(o.backend << ": " << o.skip_reason);
  REQUIRE(o.error.empty());
  REQUIRE(o.ran);
}
} // namespace

TEST_CASE(
    "a backend is told its sync group was declined", "[gfx][l3][dmacapture]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  score::gfx::interop::CaptureSyncGroup group{2};

  Plan plan;
  plan.group = &group;
  // The rung that engages cannot bind a slot the group chose, so the renderer
  // leaves this stream unsynchronised.
  plan.engagedRungBindsChosenSlot = false;

  const auto o = run(be, plan);
  requireRan(o);

  // Told at all is the whole point: without the call the backend keeps the
  // grouped slot-ownership rule while the renderer runs the ungrouped one.
  CHECK(o.told.syncEngagedCalls == 1);
  CHECK(o.told.syncEngaged == false);
}

TEST_CASE("a backend is told its sync group engaged", "[gfx][l3][dmacapture]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  score::gfx::interop::CaptureSyncGroup group{2};

  Plan plan;
  plan.group = &group;
  plan.engagedRungBindsChosenSlot = true;

  const auto o = run(be, plan);
  requireRan(o);

  CHECK(o.told.syncEngagedCalls == 1);
  CHECK(o.told.syncEngaged == true);
}

TEST_CASE(
    "a backend that offered no group is still told nothing changed",
    "[gfx][l3][dmacapture]")
{
  // The unconditional half. A backend may be written to arm its slot-return
  // policy from this call rather than to default it, and one that never offered
  // a group would then never be armed at all.
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const auto o = run(be, Plan{});
  requireRan(o);

  CHECK(o.told.syncEngagedCalls == 1);
  CHECK(o.told.syncEngaged == false);
}

TEST_CASE(
    "a declined external-image rung gets the decoder rebuilt",
    "[gfx][l3][dmacapture]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  Plan plan;
  plan.decoderNeedsExternal = true;
  plan.zeroCopyRungInits = false; // the external rung declines

  const auto o = run(be, plan);
  requireRan(o);

  // Two decoders: the external one built before the ladder ran, and the staged
  // one built after it fell through. One decoder means the node kept a decoder
  // that samples a texture nothing uploads to.
  CHECK(o.told.decodersMade == 2);
  CHECK(o.told.dropExternalCalls == 1);
  // And the request is actually dropped, not merely re-asked: a backend still
  // handing back an external-only decoder would loop or fall back to nothing.
  CHECK(o.told.stillWantsExternal == false);
  REQUIRE(o.told.engagedRung != nullptr);
  CHECK(std::string(o.told.engagedRung) == "fake-cpu-staging");
}

TEST_CASE(
    "an external-image rung that engages keeps its decoder",
    "[gfx][l3][dmacapture]")
{
  // The negative half: rebuilding the decoder costs a full re-init and a
  // host-staged copy, so it must happen only when the rung actually declined.
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  Plan plan;
  plan.decoderNeedsExternal = true;
  plan.zeroCopyRungInits = true;

  const auto o = run(be, plan);
  requireRan(o);

  CHECK(o.told.decodersMade == 1);
  CHECK(o.told.dropExternalCalls == 0);
  REQUIRE(o.told.engagedRung != nullptr);
  CHECK(std::string(o.told.engagedRung) == "fake-zerocopy");
}
