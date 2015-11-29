#include <Process/ExpandMode.hpp>
#include "Scenario/Palette/Tool.hpp"
#include "ScenarioEditionSettings.hpp"

ExpandMode Scenario::EditionSettings::expandMode() const
{
    return m_expandMode;
}

Scenario::Tool Scenario::EditionSettings::tool() const
{
    return m_tool;
}

void Scenario::EditionSettings::setExpandMode(ExpandMode expandMode)
{
    if (m_expandMode == expandMode)
        return;

    m_expandMode = expandMode;
    emit expandModeChanged(expandMode);
}

void Scenario::EditionSettings::setTool(Scenario::Tool tool)
{
    m_tool = tool;
    emit toolChanged(tool);
}

bool Scenario::EditionSettings::sequence() const
{
    return m_sequence;
}

void Scenario::EditionSettings::setSequence(bool sequence)
{
    if (m_sequence == sequence)
        return;

    m_sequence = sequence;
    emit sequenceChanged(sequence);
}
