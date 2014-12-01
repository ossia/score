#include "UnassignMessagesFromStateCommand.hpp"
using namespace iscore;


void UnassignMessagesFromStateCommand::undo()
{
}

void UnassignMessagesFromStateCommand::redo()
{
}

int UnassignMessagesFromStateCommand::id() const
{
	return -1;
}

bool UnassignMessagesFromStateCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void UnassignMessagesFromStateCommand::serializeImpl(QDataStream&)
{
}

void UnassignMessagesFromStateCommand::deserializeImpl(QDataStream&)
{
}
