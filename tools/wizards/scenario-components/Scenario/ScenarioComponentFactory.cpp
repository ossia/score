#include "ScenarioComponentFactory.hpp"
#include "ScenarioComponent.hpp"

namespace Audio
{
namespace AudioStreamEngine
{
bool ScenarioComponentFactory::matches(
        Process::ProcessModel& p,
        const DocumentPlugin&,
        const iscore::DocumentContext&) const
{
    return dynamic_cast<Scenario::ScenarioModel*>(&p);
}

ProcessComponent*
ScenarioComponentFactory::make(
        const Id<iscore::Component>& id,
        Process::ProcessModel& proc,
        const DocumentPlugin& doc,
        const iscore::DocumentContext& ctx,
        QObject* paren_objt) const
{
    return new ScenarioComponent(id, static_cast<Scenario::ScenarioModel&>(proc), doc, ctx, paren_objt);
}

}
}
