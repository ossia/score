#include "RemoveStateFromEvent.hpp"

using namespace iscore;
using namespace Scenario::Command;

void RemoveStateFromEvent::undo()
{
}

void RemoveStateFromEvent::redo()
{
}

int RemoveStateFromEvent::id() const
{
	return -1;
}

bool RemoveStateFromEvent::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveStateFromEvent::serializeImpl(QDataStream&)
{
}

void RemoveStateFromEvent::deserializeImpl(QDataStream&)
{
}
