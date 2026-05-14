#pragma once

#include <score_plugin_gfx_export.h>

#include <ossia/detail/hash_map.hpp>

#include <QImage>

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Gfx
{

/**
 * @brief Cross-RenderList content-hash dedup for decoded asset bytes.
 *
 * Lives on GfxContext, shared across all RenderLists in the session.
 * Keyed by `content_hash` (64-bit stable hash of the source bytes —
 * the canonical primitive is `ossia::hash_bytes` from
 * `ossia/detail/hash.hpp`, which dispatches to rapidhash; parsers and
 * the preprocessor produce content_hash values through that helper).
 *
 * Purpose: one decode per asset across the whole session. When two
 * glTF files reference the same `baseColor.jpg`, we decode it once
 * and reuse. Per-RenderList GpuResourceRegistries upload from the
 * cached QImage independently (Plan 09 §4.2: one decode, N uploads).
 *
 * Not the GPU-resource owner — GpuResourceRegistry does that per
 * QRhi. AssetTable only holds CPU-side bytes + format metadata
 * during the window between decode and eviction.
 *
 * # Lifecycle (Plan 09 S1)
 *
 * Three states per entry:
 *
 *   - **hot** (refcount > 0): actively held by at least one consumer.
 *     Never evicted.
 *   - **cool** (refcount == 0, still referenced in the LRU list):
 *     eviction candidate. `acquire()` resurrects it at zero cost.
 *   - **evicted**: dropped from the map. Next `acquire()` misses;
 *     the caller re-decodes and restage()s.
 *
 * Transitions:
 *   - `stage()` inserts into hot map (or no-op if already present).
 *   - `acquire()` bumps refcount and (if resurrecting) splices out
 *     of the LRU list.
 *   - `release()` decrements; at 0 the entry moves to the LRU head.
 *   - `trim(max_bytes)` pops from the LRU tail until under budget.
 *   - `maybeAutoTrim()` called periodically: reads a supplied
 *     utilization ratio and trims when above a threshold.
 *
 * Byte accounting is approximate — `sizeInBytes(DecodedAsset)` hits
 * QImage::sizeInBytes and the raw bytes vector size. Good enough
 * for budget bookkeeping without a full allocator hook.
 *
 * # Thread safety
 *
 * All public methods take `m_mutex`. Fine for the access pattern
 * (parser worker threads stage, render threads acquire/release,
 * GUI tick trims) — the mutex is held for microseconds at a time.
 */
class SCORE_PLUGIN_GFX_EXPORT AssetTable
{
public:
  /// Decoded image or raw byte payload. `image` is preferred for 2D
  /// textures (carries QImage's format metadata); `bytes` for generic
  /// buffer assets (vertex/index streams etc.).
  struct DecodedAsset
  {
    QImage image;
    std::shared_ptr<const std::vector<uint8_t>> bytes;
    std::string mime_type;
    int64_t refcount{0};
    // Approximate storage cost. Computed at stage() time; the
    // allocator may report a different value but this is the number
    // the LRU trim budgets against.
    std::size_t byte_size{0};
  };

  // For byte-range hashing use `ossia::hash_bytes` from
  // `ossia/detail/hash.hpp` — it's the canonical rapidhash-tiered
  // dispatcher that produces stable `content_hash` values across
  // the codebase. Parsers call it directly when stamping
  // `texture_source::content_hash` / `buffer_resource::content_hash`.

  AssetTable() = default;
  AssetTable(const AssetTable&) = delete;
  AssetTable& operator=(const AssetTable&) = delete;
  ~AssetTable() = default;

  /// Publish a decoded asset under its content hash. Idempotent —
  /// a second stage() with the same hash is a no-op (hash contract:
  /// same hash = same bytes).
  void stage(uint64_t content_hash, QImage image);
  void stage(
      uint64_t content_hash, std::shared_ptr<const std::vector<uint8_t>> bytes,
      std::string mime_type = {});

  /// Return a shared pointer to the decoded asset, bumping its
  /// refcount. Null when not staged. O(1) average.
  std::shared_ptr<const DecodedAsset> acquire(uint64_t content_hash);

  /// Read-through without refcount bump. The returned shared_ptr
  /// keeps the DecodedAsset alive on the caller's side even if the
  /// AssetTable evicts the entry — but does NOT prevent eviction.
  /// Suitable for the "upload once to GPU, then done" path where
  /// the consumer doesn't care if the CPU-side bytes live on.
  std::shared_ptr<const DecodedAsset> peek(uint64_t content_hash) const;

  /// Decrement refcount. At 0 the entry moves to the LRU head and
  /// is eligible for eviction on the next trim.
  void release(uint64_t content_hash);

  /// Force eviction until the cold-pool byte total is below
  /// @p max_bytes. Called explicitly by UI ("unload unused") or
  /// implicitly by maybeAutoTrim.
  /// @return bytes evicted.
  std::size_t trim(std::size_t max_bytes_budget);

  /// Called on a cadence (e.g. from the Gfx thread idle tick) to
  /// pressure-trim when the supplied utilization ratio exceeds
  /// @p high_watermark. Cost: O(n) in the LRU list when a trim
  /// fires; constant otherwise.
  ///
  /// @p utilization in [0, 1]. Compute externally from
  /// QRhiStats::usedBytes / (usedBytes + unusedBytes), or from a
  /// hard OS-level memory query.
  /// @p high_watermark default 0.80. @p target default 0.60.
  void maybeAutoTrim(
      float utilization, float high_watermark = 0.80f,
      float target = 0.60f);

  /// Debug / inspector.
  std::size_t size() const noexcept;
  /// Approx total bytes held in cold pool + hot pool.
  std::size_t totalBytes() const noexcept;
  /// Number of cold entries eligible for eviction.
  std::size_t coldCount() const noexcept;

private:
  struct Slot;  // forward

  // Linked list of cold entries, newest at head. std::list for
  // stable iterators under concurrent erase.
  using LruList = std::list<uint64_t>;

  struct Slot
  {
    std::shared_ptr<DecodedAsset> asset;
    LruList::iterator lru_it;   // valid only when refcount == 0
    bool in_lru{false};
  };

  void evictOne() noexcept;   // Pops the LRU tail. Caller holds m_mutex.

  mutable std::mutex m_mutex;
  ossia::hash_map<uint64_t, Slot> m_entries;
  LruList m_lru;              // cold entries, newest at front
  std::size_t m_total_bytes{0};
  std::size_t m_cold_bytes{0};
};

} // namespace Gfx
