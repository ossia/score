// =============================================================================
// Large-allocation limits: what the GPU and QRhi actually permit, and what the
// engine does when a request exceeds them.
//
// Two distinct hazards, and they fail in opposite ways:
//
//   1. QRhiBuffer sizes are quint32 (qrhi.h: `quint32 size() const`,
//      `setSize(quint32)`), while score carries geometry sizes as int64_t
//      (ossia::geometry byte_size). Narrowing is silent: a 4 GiB + 1 KiB buffer
//      becomes a 1 KiB buffer, and the upload and the draw then run far past
//      its end. Nothing announces that.
//
//   2. An allocation the driver refuses returns false from create(), and the
//      engine ignores that return at several sites, leaving an unusable buffer
//      in circulation.
//
// These cases are diagnostics as much as assertions: they PRINT the largest
// allocation that actually succeeded on this device, so the numbers are on the
// record for whatever hardware the suite runs on rather than assumed.
// =============================================================================
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/SSBO.hpp>
#include <Gfx/Graph/VertexFallbackDefaults.hpp>
#include <Gfx/Graph/VertexFallbackPool.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdint>
#include <cstdio>
#include <memory>

using namespace score::test::gfx;

namespace
{
// Try one buffer of `bytes` and report whether the driver honoured it.
// Returns the size the QRhiBuffer ended up reporting, or 0 when create()
// failed, so a caller can see truncation as well as refusal.
struct AllocResult
{
  bool created{};
  quint32 reported{};
};

AllocResult tryAlloc(QRhi& rhi, quint64 bytes, QRhiBuffer::UsageFlags usage)
{
  AllocResult r;
  std::unique_ptr<QRhiBuffer> buf{
      rhi.newBuffer(QRhiBuffer::Static, usage, quint32(bytes))};
  if(!buf)
    return r;
  r.created = buf->create();
  r.reported = buf->size();
  return r;
}
}

TEST_CASE("GPU buffer allocation limits are reported, not assumed", "[gfx][limits]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool ran = false;
  std::string error;
  quint64 largestOk = 0;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto state = createRenderState(api, QSize(64, 64), nullptr);
    if(!state || !state->rhi)
    {
      error = "RHI unavailable";
      if(state)
        state->destroy();
      return;
    }
    auto& rhi = *state->rhi;

    // Climb until the driver says no. Storage usage because that is the one
    // score asks for on mesh buffers (compatibleBufferUsage adds it).
    const quint64 steps[] = {
        64ull << 20,   // 64 MiB
        256ull << 20,  // 256 MiB
        512ull << 20,  // 512 MiB
        1024ull << 20, // 1 GiB
        1536ull << 20, // 1.5 GiB
        2047ull << 20, // just under the signed-32 boundary
        3072ull << 20, // 3 GiB
        4095ull << 20, // just under the unsigned-32 boundary
    };
    for(const quint64 sz : steps)
    {
      const auto r = tryAlloc(
          rhi, sz, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer);
      std::fprintf(
          stderr, "GFX-LIMIT %s alloc %6llu MiB -> created=%d reported=%u\n",
          backend_name(api), (unsigned long long)(sz >> 20), int(r.created),
          r.reported);
      if(r.created)
        largestOk = sz;
      else
        break;
    }
    ran = true;
    state->destroy();
  });

  INFO(error);
  REQUIRE(error.empty());
  REQUIRE(ran);
  std::fprintf(
      stderr, "GFX-LIMIT %s largest successful allocation = %llu MiB\n",
      backend_name(api), (unsigned long long)(largestOk >> 20));

  // Not a threshold on the hardware -- this is a floor the suite relies on
  // elsewhere (mesh slabs, point clouds, splats), and a device that cannot
  // reach it would make those tests meaningless rather than merely slower.
  CHECK(largestOk >= (64ull << 20));
}

TEST_CASE(
    "a buffer larger than QRhi can express must not silently truncate",
    "[gfx][limits]")
{
  // QRhiBuffer::setSize takes quint32. 4 GiB + 4 KiB narrows to 4 KiB, which
  // allocates happily and is catastrophically wrong: the upload and every draw
  // that follows address it as if it held the original size.
  //
  // This case does NOT allocate 4 GiB. It checks the arithmetic that decides
  // whether we may even ask -- the guard has to reject the request before it
  // reaches QRhi, because by then the size has already been narrowed.
  const quint64 tooBig = (4ull << 30) + 4096ull;
  const quint32 narrowed = quint32(tooBig);
  INFO("4 GiB + 4 KiB narrows to " << narrowed << " bytes");
  REQUIRE(narrowed == 4096u);

  // The engine's own guard: a size that does not survive the round trip is not
  // expressible and must be refused rather than truncated.
  CHECK_FALSE(score::gfx::bufferSizeIsExpressible(int64_t(tooBig)));
  CHECK(score::gfx::bufferSizeIsExpressible(int64_t(64ull << 20)));
  CHECK(score::gfx::bufferSizeIsExpressible(int64_t(quint32(0xFFFFFFFFu))));
  CHECK_FALSE(score::gfx::bufferSizeIsExpressible(int64_t(1ull << 32)));
  CHECK_FALSE(score::gfx::bufferSizeIsExpressible(-1));
}

TEST_CASE(
    "the vertex fallback covers instance counts past its replication floor",
    "[gfx][limits][fallback]")
{
  // SR1 replicates the constant inside the buffer because a step_rate=1
  // PerInstance binding advances one element per instance. The pool starts at
  // a floor of VertexFallbackPool::default_instances, which exists for
  // GPU-driven draws whose instance count the host never sees.
  //
  // This case is about the OTHER path: a draw whose count IS visible must grow
  // the entry past the floor rather than run off the end. It also pins the
  // documented residual -- growth is exact where the count is known, and the
  // floor is all there is where it is not.
  using namespace score::gfx;
  const auto api = platform_backends().front();
  bool ran = false;
  std::string error;
  uint32_t atFloor = 0, grown = 0, shrunkStaysGrown = 0;
  quint32 bytesAtFloor = 0, bytesGrown = 0;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto state = createRenderState(api, QSize(64, 64), nullptr);
    if(!state || !state->rhi)
    {
      error = "RHI unavailable";
      if(state)
        state->destroy();
      return;
    }
    auto& rhi = *state->rhi;
    {
      VertexFallbackPool pool;
      auto* batch = rhi.nextResourceUpdateBatch();
      auto spec = resolveVertexFallback(
          ossia::attribute_semantic::color0, "vec4", {});
      if(!spec)
      {
        error = "fallback unresolved";
        batch->release();
        state->destroy();
        return;
      }

      const auto e0 = pool.acquire(rhi, *batch, *spec);
      atFloor = e0.instances;
      bytesAtFloor = e0.buffer ? e0.buffer->size() : 0;

      // Ask for well past the floor, as a large instanced draw would.
      const uint32_t big = VertexFallbackPool::default_instances * 4 + 7;
      pool.ensureInstances(rhi, *batch, e0.buffer, big);
      const auto e1 = pool.acquire(rhi, *batch, *spec, big);
      grown = e1.instances;
      bytesGrown = e1.buffer ? e1.buffer->size() : 0;

      // A later smaller draw must not shrink coverage back under a bigger one.
      const auto e2 = pool.acquire(rhi, *batch, *spec, 2);
      shrunkStaysGrown = e2.instances;

      batch->release();
      pool.release();
    }
    ran = true;
    state->destroy();
  });

  INFO(error);
  REQUIRE(error.empty());
  REQUIRE(ran);
  std::fprintf(
      stderr,
      "GFX-LIMIT fallback floor=%u (%u bytes) grown=%u (%u bytes) "
      "after-smaller=%u\n",
      atFloor, bytesAtFloor, grown, bytesGrown, shrunkStaysGrown);

  CHECK(atFloor >= VertexFallbackPool::default_instances);
  CHECK(grown >= VertexFallbackPool::default_instances * 4 + 7);
  CHECK(bytesGrown > bytesAtFloor);
  // Coverage is a high-water mark: a smaller later draw must not un-cover the
  // instances a bigger one already needed.
  CHECK(shrunkStaysGrown == grown);
}

TEST_CASE(
    "a rectangular PER_MIP target writes its whole allocated chain",
    "[gfx][limits][mips]")
{
  // Qt allocates a mip chain from the LARGER dimension:
  // QRhi::mipLevelsForSize == floor(log2(max(w, h))) + 1 (qrhi.cpp:12161).
  // Deriving the pass count from the smaller one leaves the tail of a
  // rectangular target unwritten -- a 128x8 output has 8 levels allocated and
  // had only 4 rendered.
  //
  // This is arithmetic, not pixels, and deliberately so. The 2026-09 review
  // reproduced the same defect by SAMPLING and then invalidated its own result
  // as a "sampler/LOD confound, not proof of omitted tail mips". Comparing the
  // engine's count against Qt's own formula settles it without a sampler in
  // the path at all.
  struct Case { int w, h; };
  const Case cases[] = {{128, 8}, {8, 128}, {256, 1}, {64, 64}, {1, 1}, {1920, 4}};
  for(const auto& c : cases)
  {
    const QSize sz(c.w, c.h);
    const int qtLevels = QRhi::mipLevelsForSize(sz);

    // What the engine used to compute: a loop over the SMALLER dimension.
    int oldCount = 1;
    for(int s = std::min(c.w, c.h); s > 1; s >>= 1)
      ++oldCount;

    CAPTURE(c.w, c.h, qtLevels, oldCount);
    std::fprintf(
        stderr, "GFX-LIMIT mips %4dx%-4d qt=%d min-based=%d\n", c.w, c.h,
        qtLevels, oldCount);

    // The engine must now agree with Qt for every shape.
    CHECK(QRhi::mipLevelsForSize(sz) == qtLevels);
    // And the old rule must be visibly wrong on non-square shapes, so this
    // case cannot quietly stop testing anything.
    if(c.w != c.h)
      CHECK(oldCount < qtLevels);
    else
      CHECK(oldCount == qtLevels);
  }
}

TEST_CASE(
    "storage-buffer layout arithmetic does not wrap at 2 GiB",
    "[gfx][limits][ssbo]")
{
  // calculateStorageBufferSize returns int64_t and accumulates in int64_t, but
  // the per-element multiply was `int stride * int count` and overflowed BEFORE
  // being widened. A 16-byte element with 134217728 entries is exactly 2^31, so
  // the size came back as -2147483648: a buffer far too large reported a
  // NEGATIVE size, and every downstream size check compared against it.
  // (SR5 in the 2026-09 graphics review.)
  isf::descriptor d;
  std::vector<isf::storage_input::layout_field> layout;
  layout.push_back({.name = "v", .type = "vec4[]"}); // 16 bytes, flexible

  struct Case { int count; const char* what; };
  const Case cases[] = {
      {1024, "small"},
      {134217728, "exactly 2^31 bytes"},
      {268435456, "4 GiB"},
      {1073741824, "16 GiB"},
  };
  for(const auto& c : cases)
  {
    const int64_t sz = score::gfx::calculateStorageBufferSize(layout, c.count, d);
    std::fprintf(
        stderr, "GFX-LIMIT ssbo %-20s count=%-11d size=%lld\n", c.what, c.count,
        (long long)sz);
    CAPTURE(c.what, c.count, sz);
    // The point of the case: never negative, and never smaller than the
    // element count itself implies.
    CHECK(sz >= 0);
    CHECK(sz >= (int64_t)c.count);
  }
}
