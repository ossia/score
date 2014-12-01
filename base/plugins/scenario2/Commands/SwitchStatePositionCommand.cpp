#include "SwitchStatePositionCommand.hpp"
using namespace iscore;


void SwitchStatePositionCommand::undo()
{
}

void SwitchStatePositionCommand::redo()
{
}

int SwitchStatePositionCommand::id() const
{
	return 1;
}

bool SwitchStatePositionCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void SwitchStatePositionCommand::serializeImpl(QDataStream&)
{
}

void SwitchStatePositionCommand::deserializeImpl(QDataStream&)
{
}
