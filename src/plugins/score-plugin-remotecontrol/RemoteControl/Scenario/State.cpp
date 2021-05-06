#include "State.hpp"

namespace RemoteControl
{

State::State(
    Scenario::StateModel& state,
    const DocumentPlugin& doc,
    QObject* parent_comp)
    : Component{"StateComponent", parent_comp}
{
}

}
