#pragma once
#include "BaseScenarioComponent.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionAction.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/ExecutionTransaction.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Metadata.hpp>

#include <ossia/dataflow/bench_map.hpp>
#include <ossia/dataflow/dataflow_fwd.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <QTimer>

#include <memory>
#include <verdigris>
inline QDataStream& operator<<(QDataStream& i, const ossia::bench_map& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator>>(QDataStream& i, ossia::bench_map& sel)
{
  SCORE_ABORT;
  return i;
}

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
struct Queues
{
};
class ExecutionController;
class SCORE_PLUGIN_ENGINE_EXPORT DocumentPlugin final : public score::DocumentPlugin
{
  W_OBJECT(DocumentPlugin)
public:
  struct ContextData
  {
    explicit ContextData(const score::DocumentContext& ctx);

    ExecutionCommandQueue m_execQueue{1024};
    EditionCommandQueue m_editionQueue{1024};
    GCCommandQueue m_gcQueue{1024};
    std::shared_ptr<Execution::Transaction> currentTransaction;
    std::atomic_bool m_created{};

    std::shared_ptr<ossia::graph_interface> execGraph;
    std::shared_ptr<ossia::execution_state> execState;
    std::shared_ptr<ossia::bench_map> bench;
    SetupContext setupContext;

    Context context;
  };

  DocumentPlugin(const score::DocumentContext& ctx, QObject* parent);

  ~DocumentPlugin() override;
  void reload(bool forcePlay, Scenario::IntervalModel& doc);
  void clear();

  void on_documentClosing() override;
  const std::shared_ptr<BaseScenarioElement>& baseScenario() const noexcept;

  void playStartState();
  void playStopState();

  bool isPlaying() const;

  const std::shared_ptr<ContextData>& contextData() const noexcept { return m_ctxData; }
  const Context& context() const noexcept { return m_ctxData->context; }
  const ExecutionController& executionController() const noexcept;
  std::shared_ptr<ossia::audio_protocol> audioProto();

  void runAllCommands() const;

  void registerAction(ExecutionAction& act);
  const std::vector<ExecutionAction*>& actions() const noexcept { return m_actions; }

  const Execution::Settings::Model& settings;

  QPointer<Dataflow::AudioDevice> audio_device{};
  QPointer<Device::DeviceInterface> local_device{};

public:
  void finished() E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, finished)

  void slot_bench(ossia::bench_map, int64_t ns);

private:
  void on_deviceAdded(Device::DeviceInterface* device);
  void on_finished();
  void timerEvent(QTimerEvent* event) override;
  void registerDevice(ossia::net::device_base*);
  void unregisterDevice(ossia::net::device_base*);
  void makeGraph();
  void initExecState();
  void recreateBase();

  std::shared_ptr<ContextData> m_ctxData;
  std::shared_ptr<BaseScenarioElement> m_base;
  std::vector<ExecutionAction*> m_actions;

  int m_tid{};
};
}
