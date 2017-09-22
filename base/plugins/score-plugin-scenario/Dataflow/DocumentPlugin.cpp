#include <Dataflow/DocumentPlugin.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/SendStrategy.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/dataflow/audio_parameter.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <Dataflow/UI/NodeItem.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <score/document/DocumentInterface.hpp>
#include <Dataflow/UI/ConstraintNode.hpp>
namespace Dataflow
{

DocumentPlugin::DocumentPlugin(
        const score::DocumentContext& ctx,
        Id<score::DocumentPlugin> id,
        QObject* parent)
    : score::DocumentPlugin{ctx, std::move(id), "PdDocPlugin", parent}
    , scenario{ctx.model<Scenario::ScenarioDocumentModel>()}
    , window{scenario.window}
    , m_dispatcher{ctx.commandStack}
    , audioproto{new ossia::audio_protocol}
    , audio_dev{std::unique_ptr<ossia::net::protocol_base>(audioproto), "audio"}
    , midi_dev{std::make_unique<ossia::net::multiplex_protocol>(), "midi"}
{
}

void DocumentPlugin::init()
{
  midi_ins.push_back(ossia::net::create_parameter<ossia::midi_generic_parameter>(midi_dev.get_root_node(), "/0/in"));
  midi_outs.push_back(ossia::net::create_parameter<ossia::midi_generic_parameter>(midi_dev.get_root_node(), "/0/out"));

  execGraph = std::make_shared<ossia::graph>();
  audioproto->reload();

  rootPresenter = new Interval{
                  Id<score::Component>{},
                  context().model<Scenario::ScenarioDocumentModel>().baseInterval(),
                  *this,
                  this};
}

DocumentPlugin::~DocumentPlugin()
{
  audioproto->stop();
  delete rootPresenter;
}


}


