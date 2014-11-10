#include "MoveEventCommand.hpp"
using namespace iscore;


void MoveEventCommand::undo()
{
}

void MoveEventCommand::redo()
{
}

int MoveEventCommand::id() const
{
}

bool MoveEventCommand::mergeWith(const QUndoCommand* other)
{
}

void MoveEventCommand::serializeImpl(QDataStream&)
{
}

void MoveEventCommand::deserializeImpl(QDataStream&)
{
}
