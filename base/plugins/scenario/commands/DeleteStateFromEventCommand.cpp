#include "DeleteStateFromEventCommand.hpp"
using namespace iscore;


void DeleteStateFromEventCommand::undo()
{
}

void DeleteStateFromEventCommand::redo()
{
}

int DeleteStateFromEventCommand::id() const
{
}

bool DeleteStateFromEventCommand::mergeWith(const QUndoCommand* other)
{
}

void DeleteStateFromEventCommand::serializeImpl(QDataStream&)
{
}

void DeleteStateFromEventCommand::deserializeImpl(QDataStream&)
{
}
