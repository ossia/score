#include "RecordProviderFactory.hpp"
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Recording/Commands/Record.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/document/DocumentContext.hpp>
namespace Recording
{
RecordProviderFactory::~RecordProviderFactory() = default;

RecordContext::RecordContext(
        const Scenario::ProcessModel& scenar,
        Scenario::Point pt):
    context{iscore::IDocument::documentContext(scenar)},
    scenario{scenar},
    explorer{Explorer::deviceExplorerFromContext(context)},
    dispatcher{
        new Recording::Record,
        context.commandStack},
    point{pt}

{

}

}
