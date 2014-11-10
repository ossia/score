#include "DeleteRelationCommand.hpp"
using namespace iscore;


void DeleteRelationCommand::undo()
{
}

void DeleteRelationCommand::redo()
{
}

int DeleteRelationCommand::id() const
{
}

bool DeleteRelationCommand::mergeWith(const QUndoCommand* other)
{
}

void DeleteRelationCommand::serializeImpl(QDataStream&)
{
}

void DeleteRelationCommand::deserializeImpl(QDataStream&)
{
}
