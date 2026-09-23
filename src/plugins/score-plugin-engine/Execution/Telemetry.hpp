#pragma once
#include <Process/Execution/TelemetryInterface.hpp>

#include <ossia/dataflow/telemetry.hpp>

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
class AudioInlet;
class AudioOutlet;
class Port;
class ProcessModel;
}
namespace ossia
{
class graph_node;
struct audio_inlet;
struct audio_outlet;
class virtual_audio_parameter;
namespace net
{
class node_base;
}
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
class SCORE_PLUGIN_ENGINE_EXPORT Telemetry final : public TelemetryInterface
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
  //! What an audio inlet receives, once all its sources are mixed.
  Meter meterInlet(const Process::AudioInlet& inlet);
  //! Every hardware input, or every hardware output after the master gain.
  Meter meterHardwareInputs();
  Meter meterHardwareOutputs();
  //! What the graph writes to a virtual port of the audio device. The meter
  //! goes silent if the port is removed.
  Meter meterVirtualPort(ossia::virtual_audio_parameter& port);
  //! The nodes under `root` are about to be removed: the meters of their
  //! ports let go of them.
  void forgetUnder(const ossia::net::node_base& root);
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

  Playhead registerPlayhead(std::shared_ptr<ossia::telemetry::playhead_tap> tap) override;
  void release(Playhead p) override;
  const ossia::telemetry::playhead_slot* playhead(Playhead p) const noexcept override;

  //! Called by the execution when a graph was built, and when it is torn down.
  void executionStarted();
  void executionStopped();

private:
  struct Subscription
  {
    ossia::telemetry::tap_kind kind{};
    QPointer<const Process::Port> port;
    bool inlet{};
    uint32_t generation{};
    int users{};

    // Where the tap is attached in the running graph, if it is.
    std::weak_ptr<ossia::graph_node> node;
    ossia::audio_inlet* ossia_inlet{};
    ossia::audio_outlet* ossia_outlet{};
    //! A virtual port, until it is removed.
    ossia::virtual_audio_parameter* param{};
    bool attached{};
  };

  struct PlayheadSub
  {
    std::shared_ptr<ossia::telemetry::playhead_tap> tap;
    uint32_t generation{};
    bool attached{};
  };

  // A timed process node.
  struct Bench
  {
    const ossia::graph_node* node{};
    QPointer<const Process::ProcessModel> process;
    uint32_t generation{};
  };

  Meter subscribe(
      ossia::telemetry::tap_kind kind, const Process::Port* port, bool inlet,
      ossia::virtual_audio_parameter* param = nullptr);

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
  std::vector<PlayheadSub> m_playheads;
  std::vector<int> m_freePlayheads;

  void attachPlayhead(int index);

  std::shared_ptr<ossia::telemetry::arena> m_arena;
  bool m_running{};
};
}
