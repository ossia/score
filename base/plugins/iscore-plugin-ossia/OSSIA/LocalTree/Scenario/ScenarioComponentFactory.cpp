#include "ScenarioComponentFactory.hpp"
#include "ScenarioComponent.hpp"

namespace Ossia
{
namespace LocalTree
{
bool ScenarioComponentFactory::matches(
        Process::ProcessModel& p,
        const DocumentPlugin&,
        const iscore::DocumentContext&) const
{
    return dynamic_cast<Scenario::ProcessModel*>(&p);
}

ProcessComponent*
ScenarioComponentFactory::make(
        const Id<iscore::Component>& id,
        OSSIA::Node& parent,
        Process::ProcessModel& proc,
        DocumentPlugin& doc,
        const iscore::DocumentContext& ctx,
        QObject* paren_objt) const
{
    return new ScenarioComponent(id, parent, static_cast<Scenario::ProcessModel&>(proc), doc, ctx, paren_objt);
}

}
}
