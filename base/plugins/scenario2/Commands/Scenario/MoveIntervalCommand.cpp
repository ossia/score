#include "MoveIntervalCommand.hpp"
using namespace iscore;


void MoveIntervalCommand::undo()
{
}

void MoveIntervalCommand::redo()
{
}

int MoveIntervalCommand::id() const
{
	return 1;
}

bool MoveIntervalCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void MoveIntervalCommand::serializeImpl(QDataStream&)
{
}

void MoveIntervalCommand::deserializeImpl(QDataStream&)
{
}
