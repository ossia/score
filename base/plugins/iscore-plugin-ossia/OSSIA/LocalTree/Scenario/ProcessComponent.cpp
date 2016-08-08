#include "ProcessComponent.hpp"
#include <Process/Process.hpp>

Engine::LocalTree::ProcessComponent::ProcessComponent(
        ossia::net::node_base& parentNode,
        Process::ProcessModel& proc,
        DocumentPlugin& doc,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component<Scenario::GenericProcessComponent<DocumentPlugin>>{
        parentNode, proc.metadata, proc, doc, id, name, parent}
{
}

Engine::LocalTree::ProcessComponent::~ProcessComponent()
{

}

Engine::LocalTree::ProcessComponentFactory::~ProcessComponentFactory()
{

}
