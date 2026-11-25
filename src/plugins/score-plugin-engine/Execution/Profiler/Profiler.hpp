#pragma once
#include <ossia/detail/hash_map.hpp>

#include <QElapsedTimer>
#include <QJsonObject>
#include <QPointer>
#include <QString>

#include <score_plugin_engine_export.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include <verdigris>

namespace ossia
{
class graph_node;
}
namespace Process
{
class ProcessModel;
}

namespace Execution
{
//! What the audio thread accumulates between two reports.
struct AudioBenchReport
{
  struct Node
  {
    int64_t sum{};
    int64_t max{};
    int count{};
  };
  struct Spike
  {
    static constexpr int maxNodes = 8;
    int64_t totalNs{};
    double budgetNs{};
    int count{};
    std::array<std::pair<const ossia::graph_node*, int64_t>, maxNodes> nodes{};
  };
  static constexpr int maxSpikes = 16;

  ossia::hash_map<const ossia::graph_node*, Node> nodes;
  int64_t tickSum{};
  int64_t tickMax{};
  int ticks{};
  //! Sum of the ticks' durations
  double budgetNs{};
  std::array<Spike, maxSpikes> spikes{};
  int spikeCount{};

  //! Empties the report, keeping its memory.
  void reset() noexcept;
};

//! Carries reports from the audio thread to the GUI thread, the audio side
//! never allocating: a few reports go round between the two through
//! preallocated queues, and one the GUI has not given back yet is reset
//! instead.
class SCORE_PLUGIN_ENGINE_EXPORT AudioBenchChannel
{
public:
  AudioBenchChannel();
  ~AudioBenchChannel();

  //! Audio thread: the report being filled, null when none is free.
  AudioBenchReport* current() noexcept;
  //! Audio thread: hands the current report over.
  void publish() noexcept;
  //! GUI thread: `f` gets each published report, which then goes back.
  void drain(const std::function<void(const AudioBenchReport&)>& f);

private:
  struct Queues;
  std::array<AudioBenchReport, 3> m_storage;
  std::unique_ptr<Queues> m_queues;
  AudioBenchReport* m_current{};
};

//! One frame of one render output, with its nodes resolved to processes.
struct RenderFrameReport
{
  struct Node
  {
    //! The process the node belongs to, or null for nodes no process owns
    //! (outputs, previews), which are then keyed by `key` and named by `name`.
    const Process::ProcessModel* process{};
    quintptr key{};
    QString name;
    int64_t cpuNs{};
    int64_t gpuNs{-1};
    bool gpuShared{};
    int64_t targetBytes{};
  };
  const void* output{};
  int64_t frame{};
  //! steady_clock time the frame started.
  int64_t timeNs{};
  //! Only node times (an output's own passes), no frame of its own.
  bool nodesOnly{};
  int64_t cpuNs{};
  int64_t gpuNs{-1};
  std::vector<Node> nodes;
};

//! The last values of a measure, for its average and maximum.
class SCORE_PLUGIN_ENGINE_EXPORT ProfilerSeries
{
public:
  static constexpr int size = 64;
  void push(double v) noexcept;
  void clear() noexcept;
  bool empty() const noexcept { return m_count == 0; }
  double last() const noexcept;
  double avg() const noexcept;
  double max() const noexcept;

private:
  std::array<double, size> m_v{};
  int m_head{};
  int m_count{};
};

struct ProfilerEntry
{
  QPointer<const Process::ProcessModel> process;
  bool isProcess{};
  QString name;
  //! Time of the process' node in one audio tick
  ProfilerSeries audioNs;
  //! The same, in percent of the tick's duration
  ProfilerSeries audioPercent;
  //! Render-thread time of the node in one frame: update and command recording
  ProfilerSeries renderCpuNs;
  ProfilerSeries gpuNs;
  //! The last GPU time included a pass shared with other nodes.
  bool gpuShared{};
  int64_t targetBytes{};
  //! What the process header shows now.
  QString label;
};

struct ProfilerSpike
{
  enum Kind
  {
    Audio,
    Render
  } kind{};
  //! Milliseconds since the profiler was cleared, when it was received
  qint64 time{};
  //! Render spikes: the output's frame number
  int64_t frame{-1};
  double totalMs{};
  //! The tick's duration for an audio spike, the GPU time for a render one
  double otherMs{};
  std::vector<std::pair<QString, double>> top;
};

/**
 * @brief Execution timings of a document's processes, audio and render.
 *
 * Fed while the execution's Benchmark setting is on: by the audio thread,
 * through the document plug-in, and by the graphics plug-in for the render
 * outputs. Shown by the profiler panel and the process headers; read by
 * Score.profile() in scripts.
 */
class SCORE_PLUGIN_ENGINE_EXPORT Profiler final : public QObject
{
  W_OBJECT(Profiler)
public:
  explicit Profiler(QObject* parent);
  ~Profiler() override;

  void clear();
  //! Hides the costs the process headers show, keeping the measures.
  void clearLabels();

  //! A producer that has measures waiting, e.g. on the render thread, hands
  //! them over in @p pull; flush() runs them, for readers that cannot wait
  //! for the producers' timers.
  void addSource(QObject* owner, std::function<void()> pull);
  void flush();

  using ProcessOf
      = std::function<const Process::ProcessModel*(const ossia::graph_node*)>;
  void pushAudio(const AudioBenchReport& r, const ProcessOf& procOf);
  void pushRender(std::span<const RenderFrameReport> frames);

  const ossia::hash_map<quintptr, ProfilerEntry>& entries() const noexcept
  {
    return m_entries;
  }
  const std::deque<ProfilerSpike>& spikes() const noexcept { return m_spikes; }
  //! Changes whenever a spike is added or the spikes are cleared.
  uint64_t spikeGeneration() const noexcept { return m_spikeGeneration; }

  //! Percent of the tick's duration the audio thread used, per report.
  const std::deque<float>& audioHistory() const noexcept { return m_audioHistory; }
  //! Per frame, in milliseconds.
  const std::deque<float>& gpuHistory() const noexcept { return m_gpuHistory; }
  const std::deque<float>& renderCpuHistory() const noexcept { return m_cpuHistory; }
  const ProfilerSeries& audioTickNs() const noexcept { return m_audioTick; }
  const ProfilerSeries& audioLoad() const noexcept { return m_audioLoad; }
  const ProfilerSeries& frameGpuNs() const noexcept { return m_frameGpu; }
  const ProfilerSeries& frameCpuNs() const noexcept { return m_frameCpu; }
  //! Frames per second of the fastest output over its last frames, among those
  //! that rendered in the last second; 0 when unknown.
  double framesPerSecond() const noexcept;
  //! The same at steady_clock time `nowNs`.
  double framesPerSecond(int64_t nowNs) const noexcept;
  static constexpr int64_t frameTimeoutNs = 1'000'000'000;

  //! An audio tick longer than this percentage of its duration is kept as a
  //! spike; 0 disables.
  double audioSpikePercent() const noexcept;
  void setAudioSpikePercent(double p);
  //! A frame whose GPU or render-thread time exceeds this is kept as a spike;
  //! 0 disables.
  double renderSpikeMs() const noexcept { return m_renderSpikeMs; }
  void setRenderSpikeMs(double ms);
  //! Read by the audio thread.
  std::shared_ptr<std::atomic<double>> audioSpikeFraction() const noexcept
  {
    return m_audioSpikeFraction;
  }

  QJsonObject toJson() const;

  void updated() E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, updated)

private:
  ProfilerEntry& entry(quintptr key);
  void addSpike(ProfilerSpike s);
  void updateLabels(bool force);

  std::vector<std::pair<QPointer<QObject>, std::function<void()>>> m_sources;
  ossia::hash_map<quintptr, ProfilerEntry> m_entries;
  std::deque<ProfilerSpike> m_spikes;
  std::deque<float> m_audioHistory, m_gpuHistory, m_cpuHistory;
  ProfilerSeries m_audioTick, m_audioLoad, m_frameGpu, m_frameCpu;
  std::shared_ptr<std::atomic<double>> m_audioSpikeFraction;
  ossia::hash_map<const void*, std::deque<int64_t>> m_frameTimes;
  double m_renderSpikeMs{};
  QElapsedTimer m_clock;
  qint64 m_lastLabels{-1};
  uint64_t m_spikeGeneration{};
};
}

W_REGISTER_ARGTYPE(Execution::Profiler*)
