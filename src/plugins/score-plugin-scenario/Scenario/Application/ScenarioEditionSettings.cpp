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

    if (!isCreationTool(m_tool))
    {
      setLockMode(LockMode::Free);
    }

    m_tool = tool;
    toolChanged(tool);
  }
}

void Scenario::EditionSettings::setExecution(bool ex)
{
  m_execution = ex;
}

void Scenario::EditionSettings::setDefault()
{
  setTool(Scenario::Tool::Select);
  setLockMode(LockMode::Free);
}

void Scenario::EditionSettings::restoreTool()
{
  setTool(Scenario::Tool{m_previousTool});
  if (!isCreationTool(m_tool))
  {
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
