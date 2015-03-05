#include "SwitchStatePosition.hpp"

using namespace iscore;
using namespace Scenario::Command;

void SwitchStatePosition::undo()
{
}

void SwitchStatePosition::redo()
{
}

bool SwitchStatePosition::mergeWith(const Command* other)
{
    return false;
}

void SwitchStatePosition::serializeImpl(QDataStream&) const
{
}

void SwitchStatePosition::deserializeImpl(QDataStream&)
{
}
