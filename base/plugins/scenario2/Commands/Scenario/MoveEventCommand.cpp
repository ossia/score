#include "MoveEventCommand.hpp"
using namespace iscore;


void MoveEventCommand::undo()
{
}

void MoveEventCommand::redo()
{
}

int MoveEventCommand::id() const
{
	return 1;
}

bool MoveEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void MoveEventCommand::serializeImpl(QDataStream&)
{
}

void MoveEventCommand::deserializeImpl(QDataStream&)
{
}
