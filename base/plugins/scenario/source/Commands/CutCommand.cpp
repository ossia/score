#include "CutCommand.hpp"

using namespace iscore;


void CutCommand::undo()
{
}

void CutCommand::redo()
{
}

int CutCommand::id() const
{
	return 1;
}

bool CutCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void CutCommand::serializeImpl(QDataStream&)
{
}

void CutCommand::deserializeImpl(QDataStream&)
{
}
