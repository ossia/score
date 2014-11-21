#include "CreateRelationCommand.hpp"
using namespace iscore;


void CreateRelationCommand::undo()
{
}

void CreateRelationCommand::redo()
{
}

int CreateRelationCommand::id() const
{
}

bool CreateRelationCommand::mergeWith(const QUndoCommand* other)
{
}

void CreateRelationCommand::serializeImpl(QDataStream&)
{
}

void CreateRelationCommand::deserializeImpl(QDataStream&)
{
}
