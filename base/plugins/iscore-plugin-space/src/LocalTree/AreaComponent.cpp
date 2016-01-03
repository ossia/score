#include "AreaComponent.hpp"

namespace Space
{
namespace LocalTree
{

AreaComponent::AreaComponent(
        OSSIA::Node& node,
        AreaModel& area,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component{id, name, parent},
    m_thisNode{node, area.metadata, this},
    m_area{area}
{
}

AreaComponent::~AreaComponent()
{

}

const std::shared_ptr<OSSIA::Node>& AreaComponent::node() const
{ return m_thisNode.node; }

OSSIA::Node& AreaComponent::thisNode() const
{ return *node(); }

}

}

ISCORE_METADATA_IMPL(Space::LocalTree::AreaComponent)
