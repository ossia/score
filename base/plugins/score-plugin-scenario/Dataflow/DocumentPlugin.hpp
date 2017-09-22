#pragma once
#include <Dataflow/DataflowWindow.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/command/Dispatchers/QuietOngoingCommandDispatcher.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/dataflow/audio_protocol.hpp>
#include <ossia/dataflow/graph.hpp>
#include <score_plugin_scenario_export.h>
namespace Scenario { class ScenarioDocumentModel; }
namespace ossia { class audio_address; class midi_generic_parameter; }
namespace Dataflow
{
class CableItem;
class Interval;
class SCORE_PLUGIN_SCENARIO_EXPORT DocumentPlugin final
    : public score::DocumentPlugin
    , public Nano::Observer
{
  Q_OBJECT
public:
  explicit DocumentPlugin(
      const score::DocumentContext& ctx,
      Id<score::DocumentPlugin> id,
      QObject* parent);

  Scenario::ScenarioDocumentModel& scenario;
  Dataflow::DataflowWindow& window;
  void init();

  virtual ~DocumentPlugin();

  ossia::audio_protocol& audioProto() { return *audioproto; }

  void on_cableAdded(Process::Cable& c);
  void on_cableRemoving(const Process::Cable& c);

  std::shared_ptr<ossia::graph> execGraph;
  ossia::execution_state execState;


  score::QuietOngoingCommandDispatcher m_dispatcher;

  ossia::audio_protocol* audioproto{};
  mutable ossia::net::generic_device audio_dev;
  mutable ossia::net::generic_device midi_dev;

  std::vector<ossia::midi_generic_parameter*> midi_ins;
  std::vector<ossia::midi_generic_parameter*> midi_outs;
  Interval* rootPresenter{};
};
}
