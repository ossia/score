// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateComponent.hpp"
#include "MetadataParameters.hpp"
#include <ossia/editor/state/state_element.hpp>

namespace Engine
{
namespace LocalTree
{

State::State(
    ossia::net::node_base& parent,
    const Id<score::Component>& id,
    Scenario::StateModel& state,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : CommonComponent{parent, state.metadata(), doc,
                      id,     "StateComponent", parent_comp}
{
}
}
}
