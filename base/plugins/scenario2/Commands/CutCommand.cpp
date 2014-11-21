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
}

bool CutCommand::mergeWith(const QUndoCommand* other)
{
}

void CutCommand::serializeImpl(QDataStream&)
{
}

void CutCommand::deserializeImpl(QDataStream&)
{
}
