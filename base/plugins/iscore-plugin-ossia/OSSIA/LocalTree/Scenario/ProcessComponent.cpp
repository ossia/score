#include "ProcessComponent.hpp"
#include <Process/Process.hpp>

Ossia::LocalTree::ProcessComponent::ProcessComponent(
        ossia::net::Node& parentNode,
        Process::ProcessModel& proc,
        DocumentPlugin& doc,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component<Scenario::GenericProcessComponent<DocumentPlugin>>{
        parentNode, proc.metadata, proc, doc, id, name, parent}
{
}

Ossia::LocalTree::ProcessComponent::~ProcessComponent()
{

}

Ossia::LocalTree::ProcessComponentFactory::~ProcessComponentFactory()
{

}
