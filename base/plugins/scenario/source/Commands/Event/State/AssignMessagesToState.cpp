#include "AssignMessagesToState.hpp"

using namespace iscore;
using namespace Scenario::Command;

void AssignMessagesToState::undo()
{
}

void AssignMessagesToState::redo()
{
}

int AssignMessagesToState::id() const
{
    return -1;
}

bool AssignMessagesToState::mergeWith(const QUndoCommand* other)
{
    return false;
}

void AssignMessagesToState::serializeImpl(QDataStream&) const
{
}

void AssignMessagesToState::deserializeImpl(QDataStream&)
{
}
