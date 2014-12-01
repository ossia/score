#include "DeleteStateFromEventCommand.hpp"
using namespace iscore;


void DeleteStateFromEventCommand::undo()
{
}

void DeleteStateFromEventCommand::redo()
{
}

int DeleteStateFromEventCommand::id() const
{
	return -1;
}

bool DeleteStateFromEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void DeleteStateFromEventCommand::serializeImpl(QDataStream&)
{
}

void DeleteStateFromEventCommand::deserializeImpl(QDataStream&)
{
}
