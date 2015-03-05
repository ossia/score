#include "RemoveStateFromEvent.hpp"

using namespace iscore;
using namespace Scenario::Command;

void RemoveStateFromEvent::undo()
{
}

void RemoveStateFromEvent::redo()
{
}

bool RemoveStateFromEvent::mergeWith(const Command* other)
{
    return false;
}

void RemoveStateFromEvent::serializeImpl(QDataStream&) const
{
}

void RemoveStateFromEvent::deserializeImpl(QDataStream&)
{
}
