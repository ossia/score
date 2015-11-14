#include "ScenarioEditionSettings.hpp"

ExpandMode ScenarioEditionSettings::expandMode() const
{
    return m_expandMode;
}

ScenarioToolKind ScenarioEditionSettings::tool() const
{
    return m_tool;
}

void ScenarioEditionSettings::setExpandMode(ExpandMode expandMode)
{
    if (m_expandMode == expandMode)
        return;

    m_expandMode = expandMode;
    emit expandModeChanged(expandMode);
}

void ScenarioEditionSettings::setTool(ScenarioToolKind tool)
{
    if (m_tool == tool)
        return;

    m_tool = tool;
    emit toolChanged(tool);
}

bool ScenarioEditionSettings::sequence() const
{
    return m_sequence;
}

void ScenarioEditionSettings::setSequence(bool sequence)
{
    if (m_sequence == sequence)
        return;

    m_sequence = sequence;
    emit sequenceChanged(sequence);
}
