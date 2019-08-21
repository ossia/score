#include "State.hpp"

namespace RemoteControl
{

State::State(
    const Id<score::Component>& id,
    Scenario::StateModel& state,
    const DocumentPlugin& doc,
    QObject* parent_comp)
    : Component{id, "StateComponent", parent_comp}
{
}

}
