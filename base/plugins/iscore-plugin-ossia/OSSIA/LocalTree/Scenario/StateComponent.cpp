#include "StateComponent.hpp"
#include "MetadataParameters.hpp"

namespace Ossia
{
namespace LocalTree
{

State::State(
        ossia::net::node_base& parent,
        const Id<iscore::Component>& id,
        Scenario::StateModel& state,
        DocumentPlugin& doc,
        QObject* parent_comp):
    CommonComponent{parent, state.metadata, doc, id, "StateComponent", parent_comp}
{
}
}
}
