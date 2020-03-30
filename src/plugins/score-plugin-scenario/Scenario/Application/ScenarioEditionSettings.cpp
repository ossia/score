// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioEditionSettings.hpp"

#include <Process/ExpandMode.hpp>
#include <Scenario/Palette/Tool.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::EditionSettings)
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
  expandModeChanged(expandMode);
}

void Scenario::EditionSettings::setTool(Scenario::Tool tool)
{
  if (m_execution)
    return;

  if (tool != m_tool)
  {
    if (m_tool != Scenario::Tool::Playing)
      m_previousTool = m_tool;

    if (m_tool != Scenario::Tool::Create && m_tool != Scenario::Tool::CreateGraph)
    {
      setSequence(false);
      setLockMode(LockMode::Free);
    }

    m_tool = tool;
    toolChanged(tool);
  }
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
  sequenceChanged(sequence);
}

void Scenario::EditionSettings::setExecution(bool ex)
{
  m_execution = ex;
}

void Scenario::EditionSettings::setDefault()
{
  setTool(Scenario::Tool::Select);
  setSequence(false);
  setLockMode(LockMode::Free);
}

void Scenario::EditionSettings::restoreTool()
{
  setTool(Scenario::Tool{m_previousTool});
  if (m_tool != Scenario::Tool::Create && m_tool != Scenario::Tool::CreateGraph)
  {
    setSequence(false);
    setLockMode(LockMode::Free);
  }
}

LockMode Scenario::EditionSettings::lockMode() const
{
  return m_lockMode;
}

void Scenario::EditionSettings::setLockMode(LockMode lockMode)
{
  if (m_lockMode == lockMode)
    return;

  m_lockMode = lockMode;
  lockModeChanged(m_lockMode);
}
