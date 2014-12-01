#include "DeleteIntervalCommand.hpp"
using namespace iscore;


void DeleteIntervalCommand::undo()
{
}

void DeleteIntervalCommand::redo()
{
}

int DeleteIntervalCommand::id() const
{
	return 1;
}

bool DeleteIntervalCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void DeleteIntervalCommand::serializeImpl(QDataStream&)
{
}

void DeleteIntervalCommand::deserializeImpl(QDataStream&)
{
}
