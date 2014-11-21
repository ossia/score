#include "DeleteProcessCommand.hpp"
using namespace iscore;


void DeleteProcessCommand::undo()
{
}

void DeleteProcessCommand::redo()
{
}

int DeleteProcessCommand::id() const
{
}

bool DeleteProcessCommand::mergeWith(const QUndoCommand* other)
{
}

void DeleteProcessCommand::serializeImpl(QDataStream&)
{
}

void DeleteProcessCommand::deserializeImpl(QDataStream&)
{
}
