#include "AddStateToEventCommand.hpp"
using namespace iscore;


void AddStateToEventCommand::undo()
{
}

void AddStateToEventCommand::redo()
{
}

int AddStateToEventCommand::id() const
{
	return 1;
}

bool AddStateToEventCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AddStateToEventCommand::serializeImpl(QDataStream&)
{
}

void AddStateToEventCommand::deserializeImpl(QDataStream&)
{
}
