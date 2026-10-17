#pragma once
#include <Process/ExpandMode.hpp>

#include <Scenario/Palette/Tool.hpp>

#include <QObject>

#include <score_plugin_scenario_export.h>

#include <verdigris>
namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT EditionSettings final : public QObject
{
  W_OBJECT(EditionSettings)

  ExpandMode m_expandMode{ExpandMode::GrowShrink};
  Scenario::Tool m_tool{Scenario::Tool::Select};
  Scenario::Tool m_previousTool{Scenario::Tool::Select};
  LockMode m_lockMode{};
  bool m_lockHeld{false};
  bool m_execution{false};
  bool m_recordPlayback{false};

  LockMode heldLockMode() const noexcept
  {
    return m_lockHeld ? LockMode::Constrained : LockMode::Free;
  }

public:
  ExpandMode expandMode() const;
  Scenario::Tool tool() const;

  void setExpandMode(ExpandMode expandMode);
  void setTool(Scenario::Tool tool);
  void setExecution(bool ex);

  void setDefault();
  void restoreTool();

  LockMode lockMode() const;

  //! Keep in the document the values playback writes into published controls
  bool recordPlayback() const noexcept { return m_recordPlayback; }
  void setRecordPlayback(bool b);

  //! Whether the lock key (Shift) is held: the lock follows it, and the resets
  //! of the lock (tool changes, setDefault) go back to it instead of Free
  void setLockHeld(bool held);

public:
  void setLockMode(LockMode lockMode);
  W_SLOT(setLockMode);

public:
  void expandModeChanged(ExpandMode expandMode)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, expandModeChanged, expandMode)
  void toolChanged(Scenario::Tool tool)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, toolChanged, tool)

  void lockModeChanged(LockMode lockMode)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, lockModeChanged, lockMode)
  void recordPlaybackChanged(bool b)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, recordPlaybackChanged, b)

  void viewSelection() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, viewSelection)

  W_PROPERTY(Scenario::Tool, tool READ tool WRITE setTool NOTIFY toolChanged)

  W_PROPERTY(LockMode, lockMode READ lockMode WRITE setLockMode NOTIFY lockModeChanged)

  W_PROPERTY(
      ExpandMode,
      expandMode READ expandMode WRITE setExpandMode NOTIFY expandModeChanged)
};
}
