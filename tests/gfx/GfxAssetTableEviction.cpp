// =============================================================================
// UNIT — AssetTable staged-but-never-acquired entries must be evictable
// (finding R2-#12 / F12, commit a7464ffef).
//
// FINDING: AssetTable::stage() inserted a DecodedAsset with refcount==0 and
// added its bytes to m_total_bytes but did NOT put the entry into the cold LRU
// (in_lru stayed false). Only release() (meaningful only after a prior
// acquire()) ever moved an entry into the cold pool. trim()/maybeAutoTrim()
// evict EXCLUSIVELY from the cold pool. Therefore an asset that is staged and
// then dropped before any RenderList acquire()s it (undo, model swap, process
// re-instantiation before upload) is neither hot-with-a-holder nor in the LRU:
// it is counted in m_total_bytes forever and cannot be reclaimed by ANY trim,
// including trim(0). -> monotonic per-session memory growth.
//
// THE FIX: stage() now seeds the fresh refcount-0 entry directly into the cold
// LRU (in_lru=true, m_cold_bytes += byte_size) so an unconsumed stage is
// trimmable. acquire() splices it back out on first use.
//
// This is a pure unit test: it constructs an AssetTable directly and exercises
// stage / acquire / release / trim / maybeAutoTrim through the public API. The
// SAME source builds against both the pre- and post-fix engine (the method
// signatures are unchanged), so it is a clean RED->GREEN guard:
//   * pre-fix : a staged-but-never-acquired entry is NOT in the cold pool, so
//               trim(0) evicts nothing and totalBytes() stays > 0  -> RED.
//   * post-fix: trim(0) reclaims it and totalBytes() returns to 0   -> GREEN.
//
// Uses the bytes overload of stage() (byte_size == bytes->size()) so the test
// needs no real image data — the byte accounting is exact and deterministic.
// =============================================================================
#include <Gfx/AssetTable.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <vector>

using Gfx::AssetTable;

namespace
{
// A deterministic N-byte payload. estimateSize() for the bytes overload returns
// bytes->size(), so byte_size == N exactly.
std::shared_ptr<const std::vector<uint8_t>> makeBytes(std::size_t n)
{
  return std::make_shared<const std::vector<uint8_t>>(n, uint8_t{0xAB});
}
constexpr std::size_t kAssetBytes = 1024;
} // namespace

TEST_CASE(
    "AssetTable: a staged-but-never-acquired entry is evictable by trim(0)",
    "[gfx][unit][assettable][eviction]")
{
  AssetTable tbl;

  // Decode + publish, but NEVER acquire (the material was removed / undone
  // before any RenderList uploaded it).
  tbl.stage(/*content_hash*/ 0x1001, makeBytes(kAssetBytes));

  // Accounting after stage.
  REQUIRE(tbl.size() == 1);
  REQUIRE(tbl.totalBytes() == kAssetBytes);
  // The fix seeds refcount-0 stages into the cold pool immediately.
  // (Pre-fix: coldCount()==0 here -> the entry is unreachable to any trim.)
  CHECK(tbl.coldCount() == 1);

  // THE GUARD: trim to a zero cold-byte budget must reclaim the unconsumed
  // stage. Pre-fix this evicts nothing (the entry never entered the LRU).
  const std::size_t evicted = tbl.trim(0);
  CHECK(evicted == kAssetBytes);

  // Accounting must return fully to zero.
  CHECK(tbl.totalBytes() == 0);
  CHECK(tbl.size() == 0);
  CHECK(tbl.coldCount() == 0);
}

TEST_CASE(
    "AssetTable: maybeAutoTrim also reclaims an unconsumed stage under pressure",
    "[gfx][unit][assettable][eviction]")
{
  AssetTable tbl;
  tbl.stage(0x2002, makeBytes(kAssetBytes));
  REQUIRE(tbl.totalBytes() == kAssetBytes);

  // High utilization (above the default 0.80 high-watermark) must trigger a
  // pressure trim. Pre-fix m_cold_bytes==0 for the unconsumed stage, so
  // maybeAutoTrim early-returns and frees nothing.
  tbl.maybeAutoTrim(/*utilization*/ 0.99f);

  CHECK(tbl.totalBytes() == 0);
  CHECK(tbl.size() == 0);
}

TEST_CASE(
    "AssetTable: a live (acquired) entry is NOT evicted while held",
    "[gfx][unit][assettable][eviction]")
{
  AssetTable tbl;
  tbl.stage(0x3003, makeBytes(kAssetBytes));

  // Acquire -> refcount 1, spliced out of the cold pool (hot).
  auto held = tbl.acquire(0x3003);
  REQUIRE(held != nullptr);
  REQUIRE(held->refcount == 1);
  CHECK(tbl.coldCount() == 0); // hot: no longer an eviction candidate

  // Even trim(0) must NOT evict a hot entry.
  const std::size_t evicted = tbl.trim(0);
  CHECK(evicted == 0);
  CHECK(tbl.totalBytes() == kAssetBytes);
  CHECK(tbl.size() == 1);

  // The normal decode->acquire->release->trim lifecycle: release drops it to
  // the cold pool, and trim(0) then reclaims it.
  tbl.release(0x3003);
  CHECK(tbl.coldCount() == 1);
  const std::size_t evicted2 = tbl.trim(0);
  CHECK(evicted2 == kAssetBytes);
  CHECK(tbl.totalBytes() == 0);
  CHECK(tbl.size() == 0);
}

TEST_CASE(
    "AssetTable: stage() is idempotent (same hash = same bytes)",
    "[gfx][unit][assettable][eviction]")
{
  AssetTable tbl;
  tbl.stage(0x4004, makeBytes(kAssetBytes));
  tbl.stage(0x4004, makeBytes(kAssetBytes)); // no-op: same hash

  CHECK(tbl.size() == 1);
  CHECK(tbl.totalBytes() == kAssetBytes); // NOT doubled

  // Still fully reclaimable.
  tbl.trim(0);
  CHECK(tbl.totalBytes() == 0);
}
