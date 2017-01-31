#include "ProcessComponent.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <Process/Process.hpp>

Engine::LocalTree::ProcessComponent::ProcessComponent(
    ossia::net::node_base& parentNode,
    Process::ProcessModel& proc,
    DocumentPlugin& doc,
    const Id<iscore::Component>& id,
    const QString& name,
    QObject* parent)
    : Component<Scenario::GenericProcessComponent<DocumentPlugin>>{
          parentNode, proc.metadata(), proc, doc, id, name, parent}
{
}

Engine::LocalTree::ProcessComponent::~ProcessComponent() = default;
Engine::LocalTree::ProcessComponentFactory::~ProcessComponentFactory() = default;
Engine::LocalTree::ProcessComponentFactoryList::~ProcessComponentFactoryList() = default;
