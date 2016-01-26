#include "ScenarioComponentFactory.hpp"
#include "ScenarioComponent.hpp"

namespace Ossia
{
namespace LocalTree
{

const ScenarioComponentFactory::ConcreteFactoryKey&
ScenarioComponentFactory::concreteFactoryKey() const
{
    static const ConcreteFactoryKey name{"ffc0aaed-9197-4956-a966-3f54afdfb762"};
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
