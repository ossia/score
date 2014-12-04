#include "AssignMessagesToStateCommand.hpp"
using namespace iscore;


void AssignMessagesToStateCommand::undo()
{
}

void AssignMessagesToStateCommand::redo()
{
}

int AssignMessagesToStateCommand::id() const
{
	return -1;
}

bool AssignMessagesToStateCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AssignMessagesToStateCommand::serializeImpl(QDataStream&)
{
}

void AssignMessagesToStateCommand::deserializeImpl(QDataStream&)
{
}
