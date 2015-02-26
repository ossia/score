#include "Cut.hpp"

using namespace iscore;
using namespace Scenario::Command;

void Cut::undo()
{
}

void Cut::redo()
{
}

int Cut::id() const
{
    return 1;
}

bool Cut::mergeWith (const QUndoCommand* other)
{
    return false;
}

void Cut::serializeImpl (QDataStream&) const
{
}

void Cut::deserializeImpl (QDataStream&)
{
}
