#include "Profiler.hpp"

#include <ossia/detail/lockfree_queue.hpp>

#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QJsonArray>

#include <wobjectimpl.h>

#include <algorithm>
#include <chrono>
#include <cmath>

W_OBJECT_IMPL(Execution::Profiler)

namespace Execution
{
namespace
{
constexpr std::size_t historySize = 300;
constexpr std::size_t maxSpikes = 200;
constexpr int topNodes = 8;

template <typename T>
void pushHistory(std::deque<float>& h, T v)
{
  h.push_back(float(v));
  if(h.size() > historySize)
    h.pop_front();
}

QJsonObject seriesJson(const ProfilerSeries& s, double scale)
{
  QJsonObject o;
  if(s.empty())
    return o;
  o["last"] = s.last() * scale;
  o["avg"] = s.avg() * scale;
  o["max"] = s.max() * scale;
  return o;
}

QStringList location(const Process::ProcessModel& p)
{
  QStringList ret;
  for(auto* o = p.parent(); o; o = o->parent())
    if(auto itv = qobject_cast<const Scenario::IntervalModel*>(o))
      ret.prepend(itv->metadata().getName());
  return ret;
}
}

void ProfilerSeries::push(double v) noexcept
{
  m_v[m_head] = v;
  m_head = (m_head + 1) % size;
  m_count = std::min(m_count + 1, size);
}

void ProfilerSeries::clear() noexcept
{
  m_head = 0;
  m_count = 0;
}

double ProfilerSeries::last() const noexcept
{
  return m_count ? m_v[(m_head + size - 1) % size] : -1.;
}

double ProfilerSeries::avg() const noexcept
{
  if(!m_count)
    return -1.;
  double s = 0.;
  for(int i = 0; i < m_count; i++)
    s += m_v[i];
  return s / m_count;
}

double ProfilerSeries::max() const noexcept
{
  if(!m_count)
    return -1.;
  return *std::max_element(m_v.begin(), m_v.begin() + m_count);
}

Profiler::Profiler(QObject* parent)
    : QObject{parent}
    , m_audioSpikeFraction{std::make_shared<std::atomic<double>>(0.9)}
    , m_renderSpikeMs{0.}
{
  m_clock.start();
}

Profiler::~Profiler() = default;

void Profiler::clearLabels()
{
  for(auto& [k, e] : m_entries)
  {
    if(e.process && !e.label.isEmpty())
      const_cast<Process::ProcessModel*>(e.process.data())->benchmarkLabel(QString{});
    e.label.clear();
  }
}

void Profiler::clear()
{
  for(auto& [k, e] : m_entries)
    if(e.process)
      const_cast<Process::ProcessModel*>(e.process.data())->benchmarkLabel(QString{});
  m_entries.clear();
  m_spikes.clear();
  m_spikeGeneration++;
  m_audioHistory.clear();
  m_gpuHistory.clear();
  m_cpuHistory.clear();
  m_audioTick.clear();
  m_audioLoad.clear();
  m_frameGpu.clear();
  m_frameCpu.clear();
  m_frameTimes.clear();
  m_clock.restart();
  m_lastLabels = -1;
  updated();
}

void Profiler::addSource(QObject* owner, std::function<void()> pull)
{
  m_sources.emplace_back(owner, std::move(pull));
}

void Profiler::flush()
{
  std::erase_if(m_sources, [](const auto& s) { return !s.first; });
  for(auto& [owner, pull] : m_sources)
    pull();
}

ProfilerEntry& Profiler::entry(quintptr key)
{
  auto& e = m_entries[key];
  if(e.isProcess && !e.process)
    e = ProfilerEntry{};
  return e;
}

void Profiler::addSpike(ProfilerSpike s)
{
  s.time = m_clock.elapsed();
  m_spikeGeneration++;
  const auto kind = s.kind;
  m_spikes.push_back(std::move(s));
  if(std::count_if(m_spikes.begin(), m_spikes.end(), [kind](const auto& x) {
       return x.kind == kind;
     }) > std::ptrdiff_t(maxSpikes))
    m_spikes.erase(std::find_if(m_spikes.begin(), m_spikes.end(), [kind](const auto& x) {
      return x.kind == kind;
    }));
}

void AudioBenchReport::reset() noexcept
{
  nodes.clear();
  tickSum = 0;
  tickMax = 0;
  ticks = 0;
  budgetNs = 0.;
  spikeCount = 0;
}

struct AudioBenchChannel::Queues
{
  ossia::spsc_queue<AudioBenchReport*> full;
  ossia::spsc_queue<AudioBenchReport*> free;

  static bool push(ossia::spsc_queue<AudioBenchReport*>& q, AudioBenchReport* r) noexcept
  {
    if constexpr(requires { q.try_enqueue(r); })
      return q.try_enqueue(r);
    else
    {
      q.enqueue(std::move(r));
      return true;
    }
  }
};

AudioBenchChannel::AudioBenchChannel()
    : m_queues{std::make_unique<Queues>()}
{
  for(auto& r : m_storage)
  {
    r.nodes.reserve(64);
    Queues::push(m_queues->free, &r);
  }
}

AudioBenchChannel::~AudioBenchChannel() = default;

AudioBenchReport* AudioBenchChannel::current() noexcept
{
  if(!m_current)
    m_queues->free.try_dequeue(m_current);
  return m_current;
}

void AudioBenchChannel::publish() noexcept
{
  if(!m_current)
    return;
  if(Queues::push(m_queues->full, m_current))
    m_current = nullptr;
  else
    m_current->reset();
}

void AudioBenchChannel::drain(const std::function<void(const AudioBenchReport&)>& f)
{
  AudioBenchReport* r{};
  while(m_queues->full.try_dequeue(r))
  {
    f(*r);
    r->reset();
    Queues::push(m_queues->free, r);
  }
}

void Profiler::pushAudio(const AudioBenchReport& r, const ProcessOf& procOf)
{
  if(r.ticks <= 0 || r.budgetNs <= 0.)
    return;

  for(const auto& [node, acc] : r.nodes)
  {
    if(acc.count <= 0)
      continue;
    auto proc = procOf(node);
    if(!proc)
      continue;
    auto& e = entry(quintptr(proc));
    e.process = proc;
    e.isProcess = true;
    e.name = proc->prettyName();
    const double avg = double(acc.sum) / acc.count;
    e.audioNs.push(avg);
    e.audioPercent.push(100. * double(acc.sum) / r.budgetNs);
  }

  const double tick = double(r.tickSum) / r.ticks;
  m_audioTick.push(tick);
  const double load = 100. * double(r.tickSum) / r.budgetNs;
  m_audioLoad.push(load);
  pushHistory(m_audioHistory, load);

  for(int i = 0; i < r.spikeCount; i++)
  {
    const auto& sp = r.spikes[i];
    ProfilerSpike s;
    s.kind = ProfilerSpike::Audio;
    s.totalMs = sp.totalNs / 1e6;
    s.otherMs = sp.budgetNs / 1e6;
    for(int k = 0; k < sp.count; k++)
      if(auto proc = procOf(sp.nodes[k].first))
        s.top.emplace_back(proc->prettyName(), sp.nodes[k].second / 1e6);
    addSpike(std::move(s));
  }

  updateLabels(false);
  updated();
}

void Profiler::pushRender(std::span<const RenderFrameReport> frames)
{
  if(frames.empty())
    return;
  int64_t newest = 0;
  for(const auto& f : frames)
  {
    if(f.nodesOnly)
    {
      for(const auto& n : f.nodes)
      {
        auto& e = entry(n.key);
        e.name = n.name;
        e.renderCpuNs.push(double(n.cpuNs));
        if(n.gpuNs >= 0)
          e.gpuNs.push(double(n.gpuNs));
      }
      continue;
    }
    if(f.timeNs > 0)
    {
      auto& times = m_frameTimes[f.output];
      times.push_back(f.timeNs);
      if(times.size() > std::size_t(ProfilerSeries::size))
        times.pop_front();
      newest = std::max(newest, f.timeNs);
    }
    m_frameCpu.push(double(f.cpuNs));
    pushHistory(m_cpuHistory, f.cpuNs / 1e6);
    if(f.gpuNs >= 0)
    {
      m_frameGpu.push(double(f.gpuNs));
      pushHistory(m_gpuHistory, f.gpuNs / 1e6);
    }

    for(const auto& n : f.nodes)
    {
      auto& e = entry(n.process ? quintptr(n.process) : n.key);
      if(n.process)
      {
        e.process = n.process;
        e.isProcess = true;
        e.name = n.process->prettyName();
      }
      else
      {
        e.name = n.name;
      }
      e.renderCpuNs.push(double(n.cpuNs));
      if(n.gpuNs >= 0)
      {
        e.gpuNs.push(double(n.gpuNs));
        e.gpuShared = n.gpuShared;
      }
      e.targetBytes = n.targetBytes;
    }

    if(m_renderSpikeMs > 0.
       && (f.cpuNs / 1e6 > m_renderSpikeMs || f.gpuNs / 1e6 > m_renderSpikeMs))
    {
      ProfilerSpike s;
      s.kind = ProfilerSpike::Render;
      s.frame = f.frame;
      s.totalMs = f.cpuNs / 1e6;
      s.otherMs = f.gpuNs / 1e6;
      std::vector<const RenderFrameReport::Node*> sorted;
      for(const auto& n : f.nodes)
        sorted.push_back(&n);
      std::sort(sorted.begin(), sorted.end(), [](auto a, auto b) {
        return std::max(a->cpuNs, a->gpuNs) > std::max(b->cpuNs, b->gpuNs);
      });
      for(int i = 0; i < std::min<int>(topNodes, sorted.size()); i++)
      {
        const auto& n = *sorted[i];
        s.top.emplace_back(
            n.process ? n.process->prettyName() : n.name,
            std::max(n.cpuNs, n.gpuNs) / 1e6);
      }
      addSpike(std::move(s));
    }
  }

  if(newest > 0)
    ossia::erase_if(m_frameTimes, [newest](const auto& e) {
      return e.second.back() < newest - frameTimeoutNs;
    });

  updateLabels(false);
  updated();
}

double Profiler::framesPerSecond() const noexcept
{
  return framesPerSecond(std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count());
}

double Profiler::framesPerSecond(int64_t nowNs) const noexcept
{
  double best = 0.;
  for(const auto& [out, times] : m_frameTimes)
  {
    if(times.size() < 2 || times.back() < nowNs - frameTimeoutNs)
      continue;
    const double span = double(times.back() - times.front());
    if(span > 0.)
      best = std::max(best, 1e9 * double(times.size() - 1) / span);
  }
  return best;
}

double Profiler::audioSpikePercent() const noexcept
{
  return 100. * m_audioSpikeFraction->load(std::memory_order_relaxed);
}

void Profiler::setAudioSpikePercent(double p)
{
  m_audioSpikeFraction->store(std::max(0., p) / 100., std::memory_order_relaxed);
}

void Profiler::setRenderSpikeMs(double ms)
{
  m_renderSpikeMs = std::max(0., ms);
}

void Profiler::updateLabels(bool force)
{
  const auto now = m_clock.elapsed();
  if(!force && m_lastLabels >= 0 && now - m_lastLabels < 250)
    return;
  m_lastLabels = now;

  for(auto& [k, e] : m_entries)
  {
    if(!e.process)
      continue;
    QString label;
    if(!e.gpuNs.empty() && e.gpuNs.avg() >= e.renderCpuNs.avg())
      label = QStringLiteral("%1%2 ms")
                  .arg(e.gpuShared ? QStringLiteral("~") : QString{})
                  .arg(e.gpuNs.avg() / 1e6, 0, 'f', 2);
    else if(!e.renderCpuNs.empty())
      label = QStringLiteral("%1 ms").arg(e.renderCpuNs.avg() / 1e6, 0, 'f', 2);
    else if(!e.audioPercent.empty())
      label = QStringLiteral("%1%").arg(
          e.audioPercent.avg(), 0, 'f', e.audioPercent.avg() < 10. ? 2 : 1);
    if(label == e.label)
      continue;
    e.label = label;
    const_cast<Process::ProcessModel*>(e.process.data())->benchmarkLabel(label);
  }
}

QJsonObject Profiler::toJson() const
{
  QJsonObject root;
  {
    QJsonObject audio;
    audio["tickMs"] = seriesJson(m_audioTick, 1e-6);
    audio["loadPercent"] = seriesJson(m_audioLoad, 1.);
    audio["spikePercent"] = audioSpikePercent();
    root["audio"] = audio;
  }
  {
    QJsonObject render;
    render["frameCpuMs"] = seriesJson(m_frameCpu, 1e-6);
    render["frameGpuMs"] = seriesJson(m_frameGpu, 1e-6);
    render["spikeMs"] = m_renderSpikeMs;
    render["fps"] = framesPerSecond();
    root["render"] = render;
  }

  QJsonArray procs;
  for(const auto& [k, e] : m_entries)
  {
    if(e.isProcess && !e.process)
      continue;
    QJsonObject o;
    o["name"] = e.name;
    o["kind"] = e.isProcess ? QStringLiteral("process") : QStringLiteral("node");
    if(e.process)
      o["location"] = QJsonArray::fromStringList(location(*e.process));
    if(!e.audioNs.empty())
    {
      o["audioUs"] = seriesJson(e.audioNs, 1e-3);
      o["audioPercent"] = seriesJson(e.audioPercent, 1.);
    }
    if(!e.renderCpuNs.empty())
      o["renderCpuMs"] = seriesJson(e.renderCpuNs, 1e-6);
    if(!e.gpuNs.empty())
      o["gpuMs"] = seriesJson(e.gpuNs, 1e-6);
    if(e.targetBytes > 0)
      o["renderTargetBytes"] = double(e.targetBytes);
    procs.append(o);
  }
  root["processes"] = procs;

  QJsonArray spikes;
  for(const auto& s : m_spikes)
  {
    QJsonObject o;
    o["kind"] = s.kind == ProfilerSpike::Audio ? QStringLiteral("audio")
                                               : QStringLiteral("render");
    o["timeMs"] = double(s.time);
    if(s.frame >= 0)
      o["frame"] = double(s.frame);
    if(s.kind == ProfilerSpike::Audio)
    {
      o["tickMs"] = s.totalMs;
      o["budgetMs"] = s.otherMs;
    }
    else
    {
      o["cpuMs"] = s.totalMs;
      o["gpuMs"] = s.otherMs;
    }
    QJsonArray top;
    for(const auto& [name, ms] : s.top)
      top.append(QJsonObject{{"name", name}, {"ms", ms}});
    o["top"] = top;
    spikes.append(o);
  }
  root["spikes"] = spikes;
  return root;
}
}
