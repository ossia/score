// Unit tests for the interop layer's ring/fence policy surface: the pieces
// every GPU-direct video path consults before a single byte moves, and which
// therefore decide whether a rung engages, degrades, or lies about itself.
//
//   - RdmaRingDepth.hpp     the BAR1-pressure depth policy, shared by four shims
//   - DmaLockPolicy.hpp     the no-op DMA-lock policy vendors substitute
//   - InteropFence.hpp      the fence factory, and what a stub backend promises
//   - ImportedGpuBufferRing the GPU-side slot ring's config gate + rotation
//   - HostPinnedRing        the host-side slot ring on its always-available rung
//
// Everything here runs on QRhi's Null backend or on no QRhi at all. The Null
// backend is used ONLY for the bookkeeping these types do around QRhi (slot
// allocation, batch plumbing, readback completion) -- never to conclude
// anything about rendering, which it cannot answer.

#include <Gfx/Graph/interop/DmaLockPolicy.hpp>
#include <Gfx/Graph/interop/HostPinnedRing.hpp>
#include <Gfx/Graph/interop/ImportedGpuBufferRing.hpp>
#include <Gfx/Graph/interop/InteropFence.hpp>
#include <Gfx/Graph/interop/RdmaRingDepth.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <QGuiApplication>

#include <QtGlobal>

#include <QtGui/private/qrhi_p.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
// Before 6.6 the rhi/ headers do not exist and qrhi_p.h declares only the
// base QRhiInitParams; the concrete ones live in the per-backend headers.
#include <QtGui/private/qrhinull_p.h>
#endif

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

using namespace score::gfx::interop;

namespace
{

// QRhiNull's texture paths go through QPainter-on-QImage, which needs a
// QGuiApplication. Catch2 owns main(), so make one on first use.
void ensureApp()
{
  if(!qApp)
  {
    static int argc = 1;
    static char arg0[] = "InteropRingPolicyTest";
    static char* argv[] = {arg0, nullptr};
    // Deliberately leaked: a static Q*Application is destroyed from the atexit
    // chain, after main returns and Qt's own static state is gone, which faults
    // in ~QGuiApplication/~QCoreApplication on Windows. Same pattern as
    // tests/unit/InfiniteScrollerTest.cpp.
    static auto* app = new QGuiApplication(argc, argv);
    (void)app;
  }
}

std::unique_ptr<QRhi> makeNullRhi()
{
  ensureApp();
  QRhiNullInitParams params;
  return std::unique_ptr<QRhi>(QRhi::create(QRhi::Null, &params));
}

// Run one offscreen frame so QRhi consumes the batch and fires any readback
// completion callbacks.
void submit(QRhi& rhi, QRhiResourceUpdateBatch* batch)
{
  QRhiCommandBuffer* cb{};
  REQUIRE(rhi.beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
  cb->resourceUpdate(batch);
  rhi.endOffscreenFrame();
}

}

// ---------------------------------------------------------------------------
// RdmaRingDepth.hpp -- the BAR1 aperture policy
// ---------------------------------------------------------------------------

TEST_CASE("the ring depth drops exactly at the BAR1 threshold",
          "[gfx][interop][rdma][ringdepth]")
{
  constexpr RdmaRingDepths d{.full = 4, .large = 2};
  constexpr std::uint32_t threshold = 32u << 20;

  CHECK(rdmaRingDepthForFrame(0, d) == 4);
  CHECK(rdmaRingDepthForFrame(threshold - 1, d) == 4);
  // The doc says "frames >= 32 MiB": the boundary itself is the reduced depth.
  CHECK(rdmaRingDepthForFrame(threshold, d) == 2);
  CHECK(rdmaRingDepthForFrame(threshold + 1, d) == 2);

  // A UHD2 BGRA8 frame (7680x4320x4 = 132 MiB) is unambiguously large; a
  // 1080p one (7.9 MiB) unambiguously is not. Those are the two rasters the
  // policy exists to separate, so pin them by size rather than by constant.
  CHECK(rdmaRingDepthForFrame(1920u * 1080u * 4u, d) == 4);
  CHECK(rdmaRingDepthForFrame(7680u * 4320u * 4u, d) == 2);

  // The policy must not invent a depth of its own when the two agree.
  constexpr RdmaRingDepths flat{.full = 3, .large = 3};
  CHECK(rdmaRingDepthForFrame(0, flat) == 3);
  CHECK(rdmaRingDepthForFrame(1u << 30, flat) == 3);
}

// ---------------------------------------------------------------------------
// DmaLockPolicy.hpp -- the substituted no-op policy
// ---------------------------------------------------------------------------

TEST_CASE("the no-op DMA lock reports itself usable and pins nothing",
          "[gfx][interop][dmalock]")
{
  NoDmaLock policy;
  CHECK(policy.valid());

  // A policy that answered "not valid" would take the DVP shims' preflight
  // down for every vendor that fills its slot with a CPU copy.
  char buf[64]{};
  CHECK(policy.lock(buf, sizeof(buf)));
  policy.unlock(buf, sizeof(buf));

  // Null / zero-length must not be special-cased into a refusal either.
  CHECK(policy.lock(nullptr, 0));
  policy.unlock(nullptr, 0);
}

// ---------------------------------------------------------------------------
// InteropFence -- the factory, and what a stub is allowed to claim
// ---------------------------------------------------------------------------

TEST_CASE("an unsupported backend gets a fence that admits it cannot sync",
          "[gfx][interop][fence]")
{
  auto rhi = makeNullRhi();
  if(!rhi)
    SKIP("QRhi Null backend unavailable");

  auto fence = makeInteropFence(*rhi);

  // Never null: the vendor adapters dereference the result unconditionally
  // and route around it via valid().
  REQUIRE(fence != nullptr);
  CHECK_FALSE(fence->valid());

  // init() must fail rather than report a synchronisation it cannot provide.
  CHECK_FALSE(fence->init(*rhi, nullptr));
  CHECK_FALSE(fence->valid());

  // And the wait must return false, not true: a stub that returned true here
  // would tell every caller the GPU had finished writing a buffer it has not
  // even started -- the peer device would DMA a half-written frame.
  CHECK_FALSE(fence->waitOnCuda(1));

  // release() on a fence that never initialised is not a crash.
  fence->release();
  CHECK_FALSE(fence->valid());
  fence->release();
}

// ---------------------------------------------------------------------------
// ImportedGpuBufferRing -- the config gate and the rotation
// ---------------------------------------------------------------------------

TEST_CASE("the GPU ring refuses a configuration it cannot honour",
          "[gfx][interop][gpuring]")
{
  auto rhi = makeNullRhi();
  if(!rhi)
    SKIP("QRhi Null backend unavailable");

  // Any non-null value: the rejected configurations must be rejected before
  // the bridge is ever consulted, so the handle is never dereferenced.
  auto* fakeCuda = reinterpret_cast<CudaInteropContextHandle>(std::uintptr_t{1});

  SECTION("no QRhi")
  {
    ImportedGpuBufferRing ring;
    CHECK_FALSE(ring.create({.rhi = nullptr, .cudaCtx = fakeCuda,
                             .bufferSize = 4096, .slotCount = 2}));
    CHECK_FALSE(ring.valid());
  }

  SECTION("a zero-byte slot")
  {
    ImportedGpuBufferRing ring;
    CHECK_FALSE(ring.create({.rhi = rhi.get(), .cudaCtx = fakeCuda,
                             .bufferSize = 0, .slotCount = 2}));
    CHECK_FALSE(ring.valid());
  }

  SECTION("no slots")
  {
    ImportedGpuBufferRing ring;
    CHECK_FALSE(ring.create({.rhi = rhi.get(), .cudaCtx = fakeCuda,
                             .bufferSize = 4096, .slotCount = 0}));
    CHECK_FALSE(ring.valid());
  }

  SECTION("no CUDA bridge on a backend that needs one")
  {
    ImportedGpuBufferRing ring;
    CHECK_FALSE(ring.create({.rhi = rhi.get(), .cudaCtx = nullptr,
                             .bufferSize = 4096, .slotCount = 2}));
    CHECK_FALSE(ring.valid());
  }

  SECTION("a backend with no P2P path at all")
  {
    // The Null backend is not in the dispatch table; the ring must decline
    // rather than hand out slots whose gpuDevicePtr is null.
    ImportedGpuBufferRing ring;
    CHECK_FALSE(ring.create({.rhi = rhi.get(), .cudaCtx = fakeCuda,
                             .bufferSize = 4096, .slotCount = 2}));
    CHECK_FALSE(ring.valid());
    CHECK(ring.slotCount() == 0);
  }
}

TEST_CASE("a GPU ring that was never created still answers safely",
          "[gfx][interop][gpuring]")
{
  ImportedGpuBufferRing ring;
  CHECK_FALSE(ring.valid());
  CHECK(ring.slotCount() == 0);
  CHECK(ring.writeIndex() == 0);
  // advance() on an empty ring is a modulo by the slot count: it must not
  // divide by zero.
  CHECK(ring.advance() == 0);
  ring.destroy();
  ring.destroy();
  CHECK_FALSE(ring.valid());
}

// ---------------------------------------------------------------------------
// HostPinnedRing -- the CPU-staging rung, which is available everywhere
// ---------------------------------------------------------------------------

TEST_CASE("the host ring refuses a configuration it cannot honour",
          "[gfx][interop][hostring]")
{
  auto rhi = makeNullRhi();
  if(!rhi)
    SKIP("QRhi Null backend unavailable");

  HostPinnedRing ring;
  CHECK_FALSE(ring.create({.rhi = nullptr, .width = 64, .height = 64}));
  CHECK_FALSE(ring.create({.rhi = rhi.get(), .width = 0, .height = 64}));
  CHECK_FALSE(ring.create({.rhi = rhi.get(), .width = 64, .height = 0}));
  CHECK_FALSE(ring.create({.rhi = rhi.get(), .width = 64, .height = 64,
                           .slotCount = 0}));
  CHECK_FALSE(ring.valid());
  CHECK(ring.backend() == HostPinnedRingBackend::None);
  CHECK(ring.slotCount() == 0);
  CHECK(ring.advance() == 0);
  CHECK_FALSE(ring.slotReady(0));
  ring.resetSlotReady(0); // must not fault on an unbuilt ring
}

TEST_CASE("with no capability probe the host ring lands on CPU staging",
          "[gfx][interop][hostring]")
{
  auto rhi = makeNullRhi();
  if(!rhi)
    SKIP("QRhi Null backend unavailable");

  constexpr uint32_t w = 64, h = 32;
  HostPinnedRing ring;
  REQUIRE(ring.create({.rhi = rhi.get(),
                       .caps = nullptr,
                       .format = VideoPixelFormat::BGRA8,
                       .width = w,
                       .height = h,
                       .slotCount = 3,
                       .debugName = "test-host-ring"}));

  // A ring with no probe must not pick a vendor rung on faith.
  CHECK(ring.backend() == HostPinnedRingBackend::CpuStaging);
  CHECK(std::string_view{hostPinnedBackendName(ring.backend())} == "CPU-staging");
  REQUIRE(ring.valid());
  REQUIRE(ring.slotCount() == 3);

  const auto stride = defaultStride(VideoPixelFormat::BGRA8, w);
  for(std::size_t i = 0; i < ring.slotCount(); ++i)
  {
    const auto& s = ring.slot(i);
    CHECK(s.host != nullptr);
    CHECK(s.stride == stride);
    CHECK(s.size == stride * h);
    // The class contract is a 4 KiB-aligned page-locked buffer; a vendor DMA
    // engine handed a misaligned pointer refuses the pin.
    CHECK(reinterpret_cast<std::uintptr_t>(s.host) % 4096 == 0);
    CHECK_FALSE(ring.slotReady(i));
  }

  // Every slot is its own allocation.
  CHECK(ring.slot(0).host != ring.slot(1).host);
  CHECK(ring.slot(1).host != ring.slot(2).host);

  // The ring does not auto-rotate; the vendor drives it.
  CHECK(ring.writeIndex() == 0);
  CHECK(ring.advance() == 1);
  CHECK(ring.advance() == 2);
  CHECK(ring.advance() == 0);
  CHECK(ring.writeIndex() == 0);

  ring.destroy();
  CHECK_FALSE(ring.valid());
  ring.destroy();
}

TEST_CASE("an explicit stride overrides the format's own padding",
          "[gfx][interop][hostring]")
{
  auto rhi = makeNullRhi();
  if(!rhi)
    SKIP("QRhi Null backend unavailable");

  constexpr uint32_t w = 64, h = 16;
  constexpr uint32_t oversized = 4096;
  REQUIRE(oversized > defaultStride(VideoPixelFormat::BGRA8, w));

  HostPinnedRing ring;
  REQUIRE(ring.create({.rhi = rhi.get(),
                       .format = VideoPixelFormat::BGRA8,
                       .width = w,
                       .height = h,
                       .stride = oversized,
                       .slotCount = 1}));
  CHECK(ring.slot(0).stride == oversized);
  CHECK(ring.slot(0).size == std::size_t(oversized) * h);
}

TEST_CASE("the CPU rung needs a batch to upload into", "[gfx][interop][hostring]")
{
  auto rhi = makeNullRhi();
  if(!rhi)
    SKIP("QRhi Null backend unavailable");

  constexpr uint32_t w = 32, h = 16;
  HostPinnedRing ring;
  REQUIRE(ring.create({.rhi = rhi.get(),
                       .format = VideoPixelFormat::BGRA8,
                       .width = w,
                       .height = h,
                       .slotCount = 2}));

  std::unique_ptr<QRhiTexture> tex(rhi->newTexture(
      QRhiTexture::BGRA8, QSize(w, h), 1, QRhiTexture::UsedAsTransferSource));
  REQUIRE(tex->create());

  // Documented: on this backend the upload is recorded into the caller's
  // batch. Without one there is nowhere to record it, and reporting success
  // would make the vendor believe the frame reached the GPU.
  CHECK_FALSE(ring.uploadSlotToTexture(0, tex.get(), nullptr));

  CHECK_FALSE(ring.uploadSlotToTexture(0, nullptr, rhi->nextResourceUpdateBatch()));
  // Out of range is a refusal, not an out-of-bounds slot read.
  CHECK_FALSE(ring.uploadSlotToTexture(2, tex.get(), rhi->nextResourceUpdateBatch()));
  CHECK_FALSE(ring.uploadSlotToTexture(
      std::size_t(-1), tex.get(), rhi->nextResourceUpdateBatch()));

  auto* batch = rhi->nextResourceUpdateBatch();
  CHECK(ring.uploadSlotToTexture(0, tex.get(), batch));
  submit(*rhi, batch);
}

TEST_CASE("a readback in flight is not re-issued onto the same slot",
          "[gfx][interop][hostring]")
{
  auto rhi = makeNullRhi();
  if(!rhi)
    SKIP("QRhi Null backend unavailable");

  constexpr uint32_t w = 32, h = 16;
  HostPinnedRing ring;
  REQUIRE(ring.create({.rhi = rhi.get(),
                       .direction = HostPinnedDirection::TextureToBuffer,
                       .format = VideoPixelFormat::BGRA8,
                       .width = w,
                       .height = h,
                       .slotCount = 2}));

  std::unique_ptr<QRhiTexture> tex(rhi->newTexture(
      QRhiTexture::BGRA8, QSize(w, h), 1, QRhiTexture::UsedAsTransferSource));
  REQUIRE(tex->create());

  CHECK_FALSE(ring.downloadTextureToSlot(nullptr, 0, rhi->nextResourceUpdateBatch()));
  CHECK_FALSE(ring.downloadTextureToSlot(tex.get(), 9, rhi->nextResourceUpdateBatch()));
  CHECK_FALSE(ring.downloadTextureToSlot(tex.get(), 0, nullptr));

  auto* batch = rhi->nextResourceUpdateBatch();
  REQUIRE(ring.downloadTextureToSlot(tex.get(), 0, batch));
  CHECK_FALSE(ring.slotReady(0));

  // The second request for a slot whose readback has not landed must be
  // refused: QRhi holds a raw pointer to that slot's QRhiReadbackResult, and
  // re-arming it while it is in flight is the documented UB.
  CHECK_FALSE(ring.downloadTextureToSlot(tex.get(), 0, rhi->nextResourceUpdateBatch()));
  // A different slot is unaffected.
  CHECK(ring.downloadTextureToSlot(tex.get(), 1, batch));

  submit(*rhi, batch);

  CHECK(ring.slotReady(0));
  CHECK(ring.slotReady(1));

  ring.resetSlotReady(0);
  CHECK_FALSE(ring.slotReady(0));
  CHECK(ring.slotReady(1));

  // Once drained, the slot takes a new request.
  auto* batch2 = rhi->nextResourceUpdateBatch();
  CHECK(ring.downloadTextureToSlot(tex.get(), 0, batch2));
  submit(*rhi, batch2);
  CHECK(ring.slotReady(0));

  ring.resetSlotReady(0);
  ring.resetSlotReady(1);
}


// ---------------------------------------------------------------------------
// HostPinnedRing move-assignment (4ad2a48f28). Impl has no destructor and
// freeSlots() is reachable only from destroy(), so a DEFAULTED move-assign
// dropped the destination's page-locked slots (16 MB across 4 objects,
// measured) and skipped the CUDA/DVP/AMD-pinned teardown with it. operator=
// routes through destroy() now.
//
// What a normal build can assert is the ownership contract below. The LEAK
// half is only observable with LeakSanitizer: under the ASan/LSan build the
// defaulted operator= reports the destination's slots as definitely lost --
// that build is this case's negative control, since in a plain build the
// defaulted and the fixed operator= produce identical observable state.
// ---------------------------------------------------------------------------

TEST_CASE("move-assignment hands the ring over and keeps none of it",
          "[gfx][interop][hostring][move]")
{
  auto rhi = makeNullRhi();
  if(!rhi)
    SKIP("QRhi Null backend unavailable");

  constexpr uint32_t w = 32, h = 16;
  const auto make = [&](const char* name) {
    HostPinnedRing r;
    REQUIRE(r.create({.rhi = rhi.get(),
                      .caps = nullptr,
                      .format = VideoPixelFormat::BGRA8,
                      .width = w,
                      .height = h,
                      .slotCount = 2,
                      .debugName = name}));
    return r;
  };

  HostPinnedRing a = make("move-src");
  HostPinnedRing b = make("move-dst");
  REQUIRE(a.valid());
  REQUIRE(b.valid());

  void* const aSlot0 = a.slot(0).host;
  void* const aSlot1 = a.slot(1).host;
  REQUIRE(aSlot0 != nullptr);

  b = std::move(a);

  // The destination now IS the source ring: same backend, same slots, and the
  // slot memory did not move (a vendor DMA engine may already hold these
  // pointers).
  CHECK(b.valid());
  CHECK(b.backend() == HostPinnedRingBackend::CpuStaging);
  REQUIRE(b.slotCount() == 2);
  CHECK(b.slot(0).host == aSlot0);
  CHECK(b.slot(1).host == aSlot1);

  // The source keeps nothing -- no half-owned pages, no double-free ahead.
  CHECK_FALSE(a.valid());
  CHECK(a.backend() == HostPinnedRingBackend::None);
  CHECK(a.slotCount() == 0);
  a.destroy(); // inert on a moved-from ring

  // Self-move must not free the slots out from under the ring.
  auto& bref = b;
  b = std::move(bref);
  CHECK(b.valid());
  REQUIRE(b.slotCount() == 2);
  CHECK(b.slot(0).host == aSlot0);

  b.destroy();
  CHECK_FALSE(b.valid());
  b.destroy();
}
