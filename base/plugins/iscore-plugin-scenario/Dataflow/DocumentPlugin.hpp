#pragma once
#include <Dataflow/DataflowWindow.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/command/Dispatchers/QuietOngoingCommandDispatcher.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/dataflow/audio_protocol.hpp>
#include <ossia/dataflow/graph.hpp>
#include <iscore_plugin_scenario_export.h>
namespace ossia { class audio_address; class midi_generic_address; }
namespace Dataflow
{
class CableItem;
class Constraint;
class ISCORE_PLUGIN_SCENARIO_EXPORT DocumentPlugin final
    : public iscore::DocumentPlugin
    , public Nano::Observer
{
  Q_OBJECT
public:
  explicit DocumentPlugin(
      const iscore::DocumentContext& ctx,
      Id<iscore::DocumentPlugin> id,
      QObject* parent);

    void init();

  virtual ~DocumentPlugin();

  ossia::audio_protocol& audioProto() { return *audioproto; }

  void on_cableAdded(Process::Cable& c);
  void on_cableRemoving(const Process::Cable& c);

  std::shared_ptr<ossia::graph> execGraph;
  ossia::execution_state execState;


  iscore::QuietOngoingCommandDispatcher m_dispatcher;

  ossia::audio_protocol* audioproto{};
  mutable ossia::net::generic_device audio_dev;
  mutable ossia::net::generic_device midi_dev;

  std::vector<ossia::midi_generic_address*> midi_ins;
  std::vector<ossia::midi_generic_address*> midi_outs;
};
}
