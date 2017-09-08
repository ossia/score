#pragma once

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore_plugin_scenario_export.h>
class QAction;
namespace iscore
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
class TemporalScenarioPresenter;
class AddProcessDialog;
class ISCORE_PLUGIN_SCENARIO_EXPORT IntervalActions : public QObject
{
public:
  IntervalActions(ScenarioApplicationPlugin* parent);
  ~IntervalActions();

  void makeGUIElements(iscore::GUIElements& ref);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

private:
  void addProcessInInterval(const UuidKey<Process::ProcessModel>&);

  void on_showRacks();
  void on_hideRacks();

  CommandDispatcher<> dispatcher();

  ScenarioApplicationPlugin* m_parent{};
  QAction* m_addProcess{};
  QAction* m_interp{};
  QAction* m_curves{};

  QAction* m_hideRacks{};
  QAction* m_showRacks{};
};
}
