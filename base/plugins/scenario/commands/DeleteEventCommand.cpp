#include "DeleteEventCommand.hpp"
using namespace iscore;


void DeleteEventCommand::undo()
{
}

void DeleteEventCommand::redo()
{
}

int DeleteEventCommand::id() const
{
}

bool DeleteEventCommand::mergeWith(const QUndoCommand* other)
{
}

void DeleteEventCommand::serializeImpl(QDataStream&)
{
}

void DeleteEventCommand::deserializeImpl(QDataStream&)
{
}
