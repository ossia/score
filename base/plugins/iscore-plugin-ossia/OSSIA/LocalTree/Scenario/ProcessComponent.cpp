#include "ProcessComponent.hpp"

ISCORE_METADATA_IMPL(OSSIA::LocalTree::ProcessComponent)
OSSIA::LocalTree::ProcessComponent::ProcessComponent(
        OSSIA::Node& parentNode,
        Process::ProcessModel& process,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component{id, name, parent},
    m_thisNode{parentNode, process.metadata, this}
{
}

OSSIA::LocalTree::ProcessComponent::~ProcessComponent()
{

}
