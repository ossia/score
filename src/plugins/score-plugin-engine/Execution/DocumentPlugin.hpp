#pragma once
#include "BaseScenarioComponent.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionAction.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Metadata.hpp>

#include <ossia/dataflow/bench_map.hpp>
#include <ossia/dataflow/dataflow_fwd.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <memory>
#include <verdigris>

namespace ossia
{
class audio_protocol;
struct bench_map;
}
namespace Device
{
class DeviceInterface;
}
namespace score
{
class DocumentModel;
}
namespace Scenario
{
class BaseScenario;
class IntervalModel;
}
namespace Dataflow
{
class AudioDevice;
}
namespace Execution
{
class SCORE_PLUGIN_ENGINE_EXPORT DocumentPlugin final : public score::DocumentPlugin
{
  W_OBJECT(DocumentPlugin)
public:
  DocumentPlugin(const score::DocumentContext& ctx, Id<score::DocumentPlugin>, QObject* parent);

  ~DocumentPlugin() override;
  void reload(Scenario::IntervalModel& doc);
  void clear();

  void on_documentClosing() override;
  const BaseScenarioElement& baseScenario() const;
  BaseScenarioElement& baseScenario();

  bool isPlaying() const;

  const Context& context() const { return m_ctx; }
  ossia::audio_protocol& audioProto();

  void runAllCommands() const;

  void registerAction(ExecutionAction& act);
  const std::vector<ExecutionAction*>& actions() const noexcept { return m_actions; }

  const Execution::Settings::Model& settings;

  std::shared_ptr<ossia::graph_interface> execGraph;
  std::shared_ptr<ossia::execution_state> execState;
  std::shared_ptr<ossia::bench_map> bench;

  QPointer<Dataflow::AudioDevice> audio_device{};
  QPointer<Device::DeviceInterface> local_device{};

public:
  void finished() E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, finished)
  void sig_bench(ossia::bench_map arg_1, int64_t ns)
      E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, sig_bench, arg_1, ns)

public:
  void slot_bench(ossia::bench_map, int64_t ns);
  W_SLOT(slot_bench);

private:
  void on_finished();
  void timerEvent(QTimerEvent* event) override;
  void registerDevice(ossia::net::device_base*);
  void unregisterDevice(ossia::net::device_base*);
  void makeGraph();

  mutable ExecutionCommandQueue m_execQueue;
  mutable EditionCommandQueue m_editionQueue;
  Context m_ctx;
  SetupContext m_setup_ctx;
  BaseScenarioElement m_base;
  std::vector<ExecutionAction*> m_actions;
  std::atomic_bool m_created{};

  int m_tid{};
};
}
