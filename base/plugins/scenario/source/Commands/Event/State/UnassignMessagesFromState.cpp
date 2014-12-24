#include "UnassignMessagesFromState.hpp"

using namespace iscore;
using namespace Scenario::Command;


void UnassignMessagesFromState::undo()
{
}

void UnassignMessagesFromState::redo()
{
}

int UnassignMessagesFromState::id() const
{
	return -1;
}

bool UnassignMessagesFromState::mergeWith(const QUndoCommand* other)
{
	return false;
}

void UnassignMessagesFromState::serializeImpl(QDataStream&)
{
}

void UnassignMessagesFromState::deserializeImpl(QDataStream&)
{
}
