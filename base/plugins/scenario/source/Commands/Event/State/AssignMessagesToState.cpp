#include "AssignMessagesToState.hpp"

using namespace iscore;
using namespace Scenario::Command;

void AssignMessagesToState::undo()
{
}

void AssignMessagesToState::redo()
{
}

bool AssignMessagesToState::mergeWith(const Command* other)
{
    return false;
}

void AssignMessagesToState::serializeImpl(QDataStream&) const
{
}

void AssignMessagesToState::deserializeImpl(QDataStream&)
{
}
