#include "ComputationComponent.hpp"

namespace Space
{
namespace LocalTree
{

ComputationComponent::ComputationComponent(
        OSSIA::Node& node,
        ComputationModel& computation,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component{id, name, parent},
    m_thisNode{node, computation.metadata, this}
{
}

ComputationComponent::~ComputationComponent()
{

}

const std::shared_ptr<OSSIA::Node>& ComputationComponent::node() const
{ return m_thisNode.node; }

OSSIA::Node& ComputationComponent::thisNode() const
{ return *node(); }

}
}

ISCORE_METADATA_IMPL(Space::LocalTree::ComputationComponent)
