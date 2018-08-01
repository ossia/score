#pragma once
#include "BaseScenarioComponent.hpp"
#include <wobjectdefs.h>

#include <ossia/dataflow/bench_map.hpp>
#include <ossia/dataflow/dataflow_fwd.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <Execution/ExecutorContext.hpp>
#include <Process/Dataflow/Port.hpp>
#include <memory>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Metadata.hpp>

namespace ossia
{
class audio_protocol;
struct bench_map;
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
namespace Engine
{
namespace Execution
{
class SCORE_PLUGIN_ENGINE_EXPORT DocumentPlugin final
    : public score::DocumentPlugin
{
  W_OBJECT(DocumentPlugin)
public:
  DocumentPlugin(
      const score::DocumentContext& ctx,
      Id<score::DocumentPlugin>,
      QObject* parent);

  ~DocumentPlugin();
  void reload(Scenario::IntervalModel& doc);
  void clear();

  void on_documentClosing() override;
  const BaseScenarioElement& baseScenario() const;

  bool isPlaying() const;

  const Context& context() const
  {
    return m_ctx;
  }
  ossia::audio_protocol& audioProto();

  void runAllCommands() const;

  void register_node(
      const Process::ProcessModel& proc,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_node(
      const Process::ProcessModel& proc,
      const std::shared_ptr<ossia::graph_node>& node);
  void register_node(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node);
  void register_inlet(
      Process::Inlet& inlet,
      const ossia::inlet_ptr& exec,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_node(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node);
  void unregister_node_soft(
      const Process::Inlets& inlets,
      const Process::Outlets& outlets,
      const std::shared_ptr<ossia::graph_node>& node);
  void set_destination(
      const State::AddressAccessor& address, const ossia::inlet_ptr&);
  void set_destination(
      const State::AddressAccessor& address, const ossia::outlet_ptr&);

  std::shared_ptr<ossia::graph_interface> execGraph;
  std::shared_ptr<ossia::execution_state> execState;

  QPointer<Dataflow::AudioDevice> audio_device{};

  score::
      hash_map<Process::Outlet*, std::pair<ossia::node_ptr, ossia::outlet_ptr>>
          outlets;
  score::
      hash_map<Process::Inlet*, std::pair<ossia::node_ptr, ossia::inlet_ptr>>
          inlets;
  score::hash_map<Id<Process::Cable>, std::shared_ptr<ossia::graph_edge>>
      m_cables;

  score::hash_map<
      std::shared_ptr<ossia::graph_node>,
      std::vector<QMetaObject::Connection>>
      runtime_connections;

public:
  void finished() W_SIGNAL(finished);
  void sig_bench(ossia::bench_map arg_1, int64_t ns) W_SIGNAL(sig_bench, arg_1, ns);
public:
  void slot_bench(ossia::bench_map, int64_t ns); W_SLOT(slot_bench);

private:
  score::hash_map<const ossia::graph_node*, const Process::ProcessModel*>
      proc_map;

  mutable ExecutionCommandQueue m_execQueue;
  mutable ExecutionCommandQueue m_editionQueue;
  Context m_ctx;
  BaseScenarioElement m_base;

  void on_cableCreated(Process::Cable& c);
  void on_cableRemoved(const Process::Cable& c);
  void connectCable(Process::Cable& cable);
  void on_finished();

  void timerEvent(QTimerEvent* event) override;
  int m_tid{};
  void makeGraph();
};
}
}
