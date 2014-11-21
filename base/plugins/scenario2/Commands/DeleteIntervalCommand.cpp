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
}

bool DeleteIntervalCommand::mergeWith(const QUndoCommand* other)
{
}

void DeleteIntervalCommand::serializeImpl(QDataStream&)
{
}

void DeleteIntervalCommand::deserializeImpl(QDataStream&)
{
}
