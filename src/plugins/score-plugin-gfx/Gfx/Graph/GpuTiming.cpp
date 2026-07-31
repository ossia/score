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
    e.sample_count = 1;
    e.frames_since_observed = 0;
    m_entries.push_back(std::move(e));
    return;
  }

  // Ring-buffer update + rolling mean + max over the window.
  it->last_ms = ms;
  it->history[it->history_index] = ms;
  it->history_index = (it->history_index + 1) % kHistorySize;
  if(it->sample_count < kHistorySize)
    ++it->sample_count;
  it->frames_since_observed = 0;

  double sum = 0.0;
  double m = 0.0;
  for(int i = 0; i < it->sample_count; ++i)
  {
    const double v = it->history[i];
    sum += v;
    m = std::max(m, v);
  }
  it->mean_ms = sum / double(it->sample_count);
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
  // QRhi only exposes a CB-wide timestamp via lastCompletedGpuTime() —
  // there is no per-pass sub-range API. Recording that value here (under
  // a per-pass name) would cause every ScopedGpuTimer in the same frame
  // to write the identical number under different names, making the S6
  // panel show the full-frame cost against every individual pass.
  //
  // The frame-total is recorded once per frame in RenderList::renderInternal
  // under the "frame" bucket. ScopedGpuTimer's job is to emit the debug
  // marker brackets (visible in RenderDoc / Nsight) without duplicating
  // the timing attribution.
  m_cb.debugMarkBegin(QByteArray::fromRawData(m_name.data(), (qsizetype)m_name.size()));
}

ScopedGpuTimer::~ScopedGpuTimer()
{
  m_cb.debugMarkEnd();
}

} // namespace score::gfx
