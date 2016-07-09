#include "StateComponent.hpp"
#include "MetadataParameters.hpp"

namespace Ossia
{
namespace LocalTree
{

State::State(
        OSSIA::Node& parent,
        const Id<iscore::Component>& id,
        Scenario::StateModel& state,
        const State::system_t& doc,
        QObject* parent_comp):
    Component{id, "StateComponent", parent_comp},
    m_thisNode{parent, state.metadata, this}
{
    make_metadata_node(state.metadata, thisNode(), m_properties, this);
}

const iscore::Component::Key&State::key() const
{
    static const Key k{"Ossia::LocalTree::StateComponent"};
    return k;
}

State::~State()
{
    m_properties.clear();
    m_thisNode.clear();
}

}
}
