#pragma once
#include <Process/ExpandMode.hpp>
#include <QObject>
#include <Scenario/Palette/Tool.hpp>
#include <score_plugin_scenario_export.h>
namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT EditionSettings final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(ExpandMode expandMode READ expandMode WRITE setExpandMode NOTIFY
                 expandModeChanged)
  Q_PROPERTY(LockMode lockMode READ lockMode WRITE setLockMode NOTIFY lockModeChanged)
  Q_PROPERTY(Scenario::Tool tool READ tool WRITE setTool NOTIFY toolChanged)
  Q_PROPERTY(
      bool sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)

  ExpandMode m_expandMode{ExpandMode::Scale};
  Scenario::Tool m_tool{Scenario::Tool::Select};
  bool m_sequence{false};
  bool m_execution{false};

public:
  ExpandMode expandMode() const;
  Scenario::Tool tool() const;
  bool sequence() const;

  void setExpandMode(ExpandMode expandMode);
  void setTool(Scenario::Tool tool);
  void setSequence(bool sequence);
  void setExecution(bool ex);

  void setDefault();
  void restoreTool();

  LockMode lockMode() const;

public Q_SLOTS:
  void setLockMode(LockMode lockMode);

Q_SIGNALS:
  void expandModeChanged(ExpandMode expandMode);
  void toolChanged(Scenario::Tool tool);
  void sequenceChanged(bool sequence);

  void lockModeChanged(LockMode lockMode);

private:
  Scenario::Tool m_previousTool{Scenario::Tool::Select};
  LockMode m_lockMode;
};
}
