#include "Paste.hpp"

using namespace iscore;
using namespace Scenario::Command;

void Paste::undo()
{
}

void Paste::redo()
{
}

bool Paste::mergeWith(const Command* other)
{
    return false;
}

void Paste::serializeImpl(QDataStream&) const
{
}

void Paste::deserializeImpl(QDataStream&)
{
}
