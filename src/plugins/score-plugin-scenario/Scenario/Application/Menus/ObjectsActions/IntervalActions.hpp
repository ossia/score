#pragma once

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/plugins/UuidKey.hpp>

#include <QObject>

#include <score_plugin_scenario_export.h>
class QAction;
namespace score
{
struct GUIElements;
}
namespace Process
{
class ProcessModel;
class LayerContextMenuManager;
class ProcessModelFactory;
class LayerFactory;
}
namespace Scenario
{
class ScenarioApplicationPlugin;
class ScenarioPresenter;
class SCORE_PLUGIN_SCENARIO_EXPORT IntervalActions final : public QObject
{
public:
  IntervalActions(ScenarioApplicationPlugin* parent);
  ~IntervalActions();

  void makeGUIElements(score::GUIElements& ref);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

private:
  void addProcessInInterval(const UuidKey<Process::ProcessModel>&, const QString& data);

  void on_showRacks();
  void on_hideRacks();

  CommandDispatcher<> dispatcher();

  ScenarioApplicationPlugin* m_parent{};
  QAction* m_addProcess{};

  QAction* m_hideRacks{};
  QAction* m_showRacks{};
};
}
