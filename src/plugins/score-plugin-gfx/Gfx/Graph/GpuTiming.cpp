#include <Gfx/Graph/GpuTiming.hpp>

#include <algorithm>

namespace score::gfx
{

void GpuTimings::record(std::string_view name, double ms) noexcept
{
  // Samples of 0 typically mean "backend doesn't support timestamps" or
  // "resolved value not yet available" — don't pollute the rolling
  // mean with those. An explicit clear happens via reset().
  if(ms <= 0.0)
    return;

  std::lock_guard lk{m_mutex};

  auto it = std::find_if(
      m_entries.begin(), m_entries.end(),
      [&](const Entry& e) { return e.name == name; });

  if(it == m_entries.end())
  {
    Entry e;
    e.name.assign(name);
    e.history.fill(0.0);
    e.last_ms = ms;
    e.mean_ms = ms;
    e.max_ms = ms;
    e.history[0] = ms;
    e.history_index = 1 % kHistorySize;
    e.frames_since_observed = 0;
    m_entries.push_back(std::move(e));
    return;
  }

  // Ring-buffer update + rolling mean + max over the window.
  it->last_ms = ms;
  it->history[it->history_index] = ms;
  it->history_index = (it->history_index + 1) % kHistorySize;
  it->frames_since_observed = 0;

  double sum = 0.0;
  double m = 0.0;
  for(double v : it->history)
  {
    sum += v;
    m = std::max(m, v);
  }
  it->mean_ms = sum / double(kHistorySize);
  it->max_ms = m;
}

void GpuTimings::tickFrame() noexcept
{
  std::lock_guard lk{m_mutex};
  for(auto& e : m_entries)
    ++e.frames_since_observed;

  // Drop entries not observed for a while — nodes get reconfigured,
  // passes come and go, keeping stale ghosts in the panel is noise.
  m_entries.erase(
      std::remove_if(
          m_entries.begin(), m_entries.end(),
          [](const Entry& e) {
            return e.frames_since_observed > kStaleThreshold;
          }),
      m_entries.end());
}

std::vector<GpuTimings::Entry> GpuTimings::snapshot() const
{
  std::lock_guard lk{m_mutex};
  return m_entries;
}

void GpuTimings::reset() noexcept
{
  std::lock_guard lk{m_mutex};
  m_entries.clear();
}

ScopedGpuTimer::ScopedGpuTimer(
    QRhiCommandBuffer& cb, GpuTimings& timings, std::string_view name)
    : m_cb{cb}
    , m_timings{timings}
    , m_name{name}
{
  // Read the CB-wide GPU time for the previously-completed frame and
  // attribute it to this pass. This is the one-frame-stale path that
  // QRhi supports out of the box. A future per-query-range API would
  // let us be more precise; today this is what we have.
  //
  // The CB keeps its own internal timestamp query pair; the returned
  // value is independent of the marker we emit here. It's safe to
  // read every frame — returns 0 until the first completion, then
  // stable millisecond deltas.
  const double ms = m_cb.lastCompletedGpuTime();
  if(ms > 0.0)
    m_timings.record(m_name, ms);

  // Emit a debug marker so RenderDoc / Nsight captures show the pass
  // boundary even when timestamps aren't available.
  m_cb.debugMarkBegin(QByteArray::fromRawData(m_name.data(), (qsizetype)m_name.size()));
}

ScopedGpuTimer::~ScopedGpuTimer()
{
  m_cb.debugMarkEnd();
}

} // namespace score::gfx
