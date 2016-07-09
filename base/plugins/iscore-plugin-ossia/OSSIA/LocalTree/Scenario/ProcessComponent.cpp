#include "ProcessComponent.hpp"
#include <Process/Process.hpp>
Ossia::LocalTree::ProcessComponent::ProcessComponent(
        OSSIA::Node& parentNode,
        Process::ProcessModel& proc,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component{id, name, parent},
    process{proc},
    m_thisNode{parentNode, proc.metadata, this}
{
}

Ossia::LocalTree::ProcessComponent::~ProcessComponent()
{

}

Ossia::LocalTree::ProcessComponentFactory::~ProcessComponentFactory()
{

}
