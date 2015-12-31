#include "ProcessComponent.hpp"

ISCORE_METADATA_IMPL(Ossia::LocalTree::ProcessComponent)
Ossia::LocalTree::ProcessComponent::ProcessComponent(
        OSSIA::Node& parentNode,
        Process::ProcessModel& process,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component{id, name, parent},
    m_thisNode{parentNode, process.metadata, this}
{
}

Ossia::LocalTree::ProcessComponent::~ProcessComponent()
{

}

Ossia::LocalTree::ProcessComponentFactory::~ProcessComponentFactory()
{

}
