#pragma once
#include <ossia/dataflow/telemetry.hpp>

#include <QObject>
#include <QPointer>

#include <score_plugin_engine_export.h>

#include <memory>
#include <vector>

#include <verdigris>

namespace score
{
struct DocumentContext;
}
namespace Process
{
class AudioOutlet;
class ProcessModel;
}
namespace ossia
{
class graph_node;
struct audio_outlet;
}
namespace Execution
{
class DocumentPlugin;

/**
 * The interface side of ossia::telemetry: what the execution tells the views,
 * for as long as the execution-update setting allows it.
 *
 * Views ask for a meter and keep the handle; the meters stay theirs across
 * stops and starts. While the execution runs and updates are enabled, the
 * levels are read at the document's coarse update rate and updated() fires.
 */
class SCORE_PLUGIN_ENGINE_EXPORT Telemetry final : public QObject
{
  W_OBJECT(Telemetry)
public:
  struct Meter
  {
    int index{-1};
    uint32_t generation{};
    explicit operator bool() const noexcept { return index >= 0; }
  };

  Telemetry(const score::DocumentContext& ctx, DocumentPlugin& plug);
  ~Telemetry() override;

  //! What an audio outlet carries, after its gain and pan.
  Meter meterOutlet(const Process::AudioOutlet& outlet);
  //! Every hardware input, or every hardware output after the master gain.
  Meter meterHardwareInputs();
  Meter meterHardwareOutputs();
  void release(Meter m);

  //! The levels since the previous update, or nullptr when there are none:
  //! execution stopped, updates disabled, or the metered point not running.
  const ossia::telemetry::meter_levels* levels(Meter m) const noexcept;

  //! Sample rate of the latest update, for turning frames into time.
  int sampleRate() const noexcept;

  //! Share of the real time a process took to run since the previous update,
  //! 1 being all of it; negative when it is not measured. Processes are
  //! measured while the benchmark setting is on.
  double cpuLoad(const Process::ProcessModel& proc) const noexcept;

  //! Called by the execution when a graph was built, and when it is torn down.
  void executionStarted();
  void executionStopped();

  void updated() E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, updated)

private:
  struct Subscription
  {
    ossia::telemetry::tap_kind kind{};
    QPointer<const Process::AudioOutlet> outlet;
    uint32_t generation{};
    int users{};

    // Where the tap is attached in the running graph, if it is.
    std::weak_ptr<ossia::graph_node> node;
    ossia::audio_outlet* ossia_outlet{};
    bool attached{};
  };

  // A timed process node.
  struct Bench
  {
    const ossia::graph_node* node{};
    QPointer<const Process::ProcessModel> process;
    uint32_t generation{};
  };

  Meter subscribe(ossia::telemetry::tap_kind kind, const Process::AudioOutlet* outlet);
  bool benchEnabled() const noexcept;
  //! Follows the process nodes of the running graph; false when the arena
  //! has no room left for them.
  bool syncBenches();
  void reportBenches();
  bool enabled() const noexcept;
  void rebuild();
  void teardown();
  void attach(int index);
  void detach(int index);
  void read();
  uint64_t publishInterval() const noexcept;

  const score::DocumentContext& m_context;
  DocumentPlugin& m_plugin;
  std::vector<Subscription> m_subs;
  std::vector<int> m_free;
  std::vector<Bench> m_benches;
  std::vector<int> m_freeBenches;

  std::shared_ptr<ossia::telemetry::arena> m_arena;
  bool m_running{};
};
}
