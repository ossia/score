#include "SwitchStatePosition.hpp"

using namespace iscore;
using namespace Scenario::Command;

void SwitchStatePosition::undo()
{
}

void SwitchStatePosition::redo()
{
}

int SwitchStatePosition::id() const
{
	return 1;
}

bool SwitchStatePosition::mergeWith(const QUndoCommand* other)
{
	return false;
}

void SwitchStatePosition::serializeImpl(QDataStream&)
{
}

void SwitchStatePosition::deserializeImpl(QDataStream&)
{
}
