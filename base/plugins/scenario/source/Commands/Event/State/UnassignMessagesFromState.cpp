#include "UnassignMessagesFromState.hpp"

using namespace iscore;
using namespace Scenario::Command;


void UnassignMessagesFromState::undo()
{
}

void UnassignMessagesFromState::redo()
{
}

bool UnassignMessagesFromState::mergeWith(const Command* other)
{
    return false;
}

void UnassignMessagesFromState::serializeImpl(QDataStream&) const
{
}

void UnassignMessagesFromState::deserializeImpl(QDataStream&)
{
}
