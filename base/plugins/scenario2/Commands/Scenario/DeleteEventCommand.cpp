#include "DeleteEventCommand.hpp"
using namespace iscore;


void DeleteEventCommand::undo()
{
}

void DeleteEventCommand::redo()
{
}

int DeleteEventCommand::id() const
{
	return 1;
}

bool DeleteEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void DeleteEventCommand::serializeImpl(QDataStream&)
{
}

void DeleteEventCommand::deserializeImpl(QDataStream&)
{
}
