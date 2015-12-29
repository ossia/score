#include "ScenarioComponentFactory.hpp"
#include "ScenarioComponent.hpp"

namespace OSSIA
{
namespace LocalTree
{

const ScenarioComponentFactory::factory_key_type&
ScenarioComponentFactory::key_impl() const
{
    static const factory_key_type name{"ScenarioComponentFactory"};
    return name;
}

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
        OSSIA::Node& parent,
        Process::ProcessModel& proc,
        const DocumentPlugin& doc,
        const iscore::DocumentContext& ctx,
        QObject* paren_objt) const
{
    return new ScenarioComponent(id, parent, static_cast<Scenario::ScenarioModel&>(proc), doc, ctx, paren_objt);
}

}
}
