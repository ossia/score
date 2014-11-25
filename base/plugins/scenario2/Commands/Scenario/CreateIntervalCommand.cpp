#include "CreateIntervalCommand.hpp"
using namespace iscore;


void CreateIntervalCommand::undo()
{
}

void CreateIntervalCommand::redo()
{
}

int CreateIntervalCommand::id() const
{
}

bool CreateIntervalCommand::mergeWith(const QUndoCommand* other)
{
}

void CreateIntervalCommand::serializeImpl(QDataStream&)
{
}

void CreateIntervalCommand::deserializeImpl(QDataStream&)
{
}
