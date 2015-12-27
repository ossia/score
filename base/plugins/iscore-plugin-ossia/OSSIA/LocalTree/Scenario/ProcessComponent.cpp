#include "ProcessComponent.hpp"

ISCORE_METADATA_IMPL(OSSIA::LocalTree::ProcessComponent)
OSSIA::LocalTree::ProcessComponent::ProcessComponent(
        const std::shared_ptr<OSSIA::Node>& node,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component{id, name, parent},
    m_thisNode{node}
{
}

OSSIA::LocalTree::ProcessComponent::~ProcessComponent()
{

}
