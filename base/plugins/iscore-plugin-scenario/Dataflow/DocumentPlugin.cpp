#include <Dataflow/DocumentPlugin.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/SendStrategy.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/dataflow/audio_address.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
/*
#include <Dataflow/UI/ScenarioNode.hpp>
#include <Dataflow/UI/NodeItem.hpp>
#include <Dataflow/UI/ConstraintNode.hpp>
*/
#include <Dataflow/UI/NodeItem.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
namespace Dataflow
{

DocumentPlugin::DocumentPlugin(
        const iscore::DocumentContext& ctx,
        Id<iscore::DocumentPlugin> id,
        QObject* parent):
    iscore::DocumentPlugin{ctx, std::move(id), "PdDocPlugin", parent},
    m_dispatcher{ctx.commandStack},
    audioproto{new ossia::audio_protocol},
    audio_dev{std::unique_ptr<ossia::net::protocol_base>(audioproto), "audio"},
    midi_dev{std::make_unique<ossia::net::multiplex_protocol>(), "midi"}
{
}

void DocumentPlugin::init()
{
  midi_ins.push_back(ossia::net::create_address<ossia::midi_generic_address>(midi_dev.get_root_node(), "/0/in"));
  midi_outs.push_back(ossia::net::create_address<ossia::midi_generic_address>(midi_dev.get_root_node(), "/0/out"));

  execGraph = std::make_shared<ossia::graph>();
  audioproto->reload();
}

DocumentPlugin::~DocumentPlugin()
{
  audioproto->stop();
}


}


