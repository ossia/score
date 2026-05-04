#pragma once
#include <score_plugin_gfx_export.h>

#include <QtGui/private/qrhi_p.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace score::gfx
{
/**
 * @brief Per-pass GPU timing collector (Plan 09 S0 / S6).
 *
 * QRhi exposes only a single `QRhiCommandBuffer::lastCompletedGpuTime()`
 * value — the elapsed GPU time of the most recently COMPLETED frame on
 * that CB. Internally QRhi wraps the CB with a timestamp query pair and
 * returns the delta in milliseconds. This class gives us per-pass
 * granularity via scoped markers: every `ScopedGpuTimer` pushes a
 * debug marker pair around its `beginPass` / `endPass` and reads
 * `lastCompletedGpuTime()` ONE FRAME LATER, attributing the delta to
 * the named pass.
 *
 * Results are always one frame late (the GPU must complete, then the
 * CPU reads back the resolved timestamp). Callers expecting live
 * numbers should treat the read as "previous frame's time".
 *
 * The collector is per-RenderList. It accumulates a rolling mean over
 * the last N frames and exposes a snapshot via `timingsLastFrame()`
 * for the S6 observability panel.
 *
 * Thread model: all public methods are called from the Gfx thread.
 * The panel's read path takes a shared lock; writers hold an exclusive
 * lock during update. Lock contention is negligible (one update/frame,
 * one read/ui-tick).
 */
class SCORE_PLUGIN_GFX_EXPORT GpuTimings
{
public:
  static constexpr int kHistorySize = 64;

  struct Entry
  {
    std::string name;
    double last_ms{0.0};
    double mean_ms{0.0};
    double max_ms{0.0};
    std::array<double, kHistorySize> history{};
    int history_index{0};
    int frames_since_observed{0};
  };

  GpuTimings() = default;
  GpuTimings(const GpuTimings&) = delete;
  GpuTimings& operator=(const GpuTimings&) = delete;

  /**
   * @brief Record an observation for a named pass.
   *
   * @p ms may be 0 when caps.timestamps is false or when the backend
   * hasn't resolved a timestamp yet. Zero samples skip the rolling
   * mean update.
   */
  void record(std::string_view name, double ms) noexcept;

  /**
   * @brief Tick once per frame. Entries not observed for more than
   *        `kStaleThreshold` frames are dropped.
   */
  void tickFrame() noexcept;

  /**
   * @brief Snapshot of all entries for the observability panel.
   *
   * Returns a copy so the caller doesn't need to hold a lock while
   * iterating. Cost: O(n_entries); typical n ≤ 32.
   */
  std::vector<Entry> snapshot() const;

  /**
   * @brief Reset all state. Called on RenderList re-init.
   */
  void reset() noexcept;

private:
  static constexpr int kStaleThreshold = 120;   // drop entries after 2s at 60fps

  mutable std::mutex m_mutex;
  std::vector<Entry> m_entries;
};

/**
 * @brief RAII helper that names the region between `beginPass` and
 * `endPass` for GPU timing attribution.
 *
 * The actual timestamp comes from the CB's lastCompletedGpuTime() in
 * the NEXT frame — this is a QRhi constraint. ScopedGpuTimer only
 * emits a debug marker pair so the pass boundary is visible in
 * captures (RenderDoc etc.) AND stamps the RenderList-scoped
 * GpuTimings collector with the previous frame's GPU time on the
 * first marker hit. This one-frame-stale read is what `lastCompletedGpuTime`
 * exposes — not fixable without adding per-pass timestamp queries,
 * which QRhi doesn't expose.
 *
 * In practice this means: the S6 panel shows per-frame whole-CB GPU
 * time under the outermost pass's name. For finer granularity, users
 * can wrap individual passes and read the data from debug captures.
 * A future QRhi feature may expose sub-CB query ranges.
 */
class SCORE_PLUGIN_GFX_EXPORT ScopedGpuTimer
{
public:
  ScopedGpuTimer(
      QRhiCommandBuffer& cb, GpuTimings& timings, std::string_view name);
  ~ScopedGpuTimer();

  ScopedGpuTimer(const ScopedGpuTimer&) = delete;
  ScopedGpuTimer& operator=(const ScopedGpuTimer&) = delete;

private:
  QRhiCommandBuffer& m_cb;
  GpuTimings& m_timings;
  std::string m_name;
};

} // namespace score::gfx
