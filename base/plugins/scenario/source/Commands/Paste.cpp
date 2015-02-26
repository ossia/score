#include "Paste.hpp"

using namespace iscore;
using namespace Scenario::Command;

void Paste::undo()
{
}

void Paste::redo()
{
}

int Paste::id() const
{
    return 1;
}

bool Paste::mergeWith(const QUndoCommand* other)
{
    return false;
}

void Paste::serializeImpl(QDataStream&) const
{
}

void Paste::deserializeImpl(QDataStream&)
{
}
