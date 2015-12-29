#include "StateComponent.hpp"
#include "MetadataParameters.hpp"

namespace OSSIA
{
namespace LocalTree
{

StateComponent::StateComponent(
        Node& parent,
        const Id<iscore::Component>& id,
        StateModel& state,
        const StateComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_comp):
    Component{id, "StateComponent", parent_comp},
    m_thisNode{parent, state.metadata, this}
{
    make_metadata_node(state.metadata, thisNode(), m_properties, this);
}

const iscore::Component::Key&StateComponent::key() const
{
    static const Key k{"OSSIA::LocalTree::StateComponent"};
    return k;
}

StateComponent::~StateComponent()
{
}

}
}
