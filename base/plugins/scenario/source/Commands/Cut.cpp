#include "Cut.hpp"

using namespace iscore;
using namespace Scenario::Command;

void Cut::undo()
{
}

void Cut::redo()
{
}

bool Cut::mergeWith(const Command* other)
{
    return false;
}

void Cut::serializeImpl(QDataStream&) const
{
}

void Cut::deserializeImpl(QDataStream&)
{
}
