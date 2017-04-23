#pragma once

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore_plugin_scenario_export.h>
class QAction;
namespace Process
{
class LayerContextMenuManager;
}
namespace iscore
{
struct GUIElements;
}
namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
namespace Command
{
class TriggerCommandFactoryList;
}
class ISCORE_PLUGIN_SCENARIO_EXPORT EventActions : public QObject
{
public:
  EventActions(ScenarioApplicationPlugin* parent);

  void makeGUIElements(iscore::GUIElements& ref);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

private:
  void addTriggerToTimeNode();
  void removeTriggerFromTimeNode();
  void addCondition();
  void removeCondition();

  CommandDispatcher<> dispatcher();

  ScenarioApplicationPlugin* m_parent{};
  QAction* m_addTrigger{};
  QAction* m_removeTrigger{};

  QAction* m_addCondition{};
  QAction* m_removeCondition{};

  const Command::TriggerCommandFactoryList& m_triggerCommandFactory;
};
}
