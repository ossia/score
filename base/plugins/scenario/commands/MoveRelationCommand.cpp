#include "MoveRelationCommand.hpp"
using namespace iscore;


void MoveRelationCommand::undo()
{
}

void MoveRelationCommand::redo()
{
}

int MoveRelationCommand::id() const
{
}

bool MoveRelationCommand::mergeWith(const QUndoCommand* other)
{
}

void MoveRelationCommand::serializeImpl(QDataStream&)
{
}

void MoveRelationCommand::deserializeImpl(QDataStream&)
{
}
