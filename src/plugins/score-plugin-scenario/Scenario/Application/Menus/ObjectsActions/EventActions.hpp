#pragma once

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QObject>

#include <score_plugin_scenario_export.h>
class QAction;
namespace Process
{
class LayerContextMenuManager;
}
namespace score
{
struct GUIElements;
}
namespace Scenario
{
class ScenarioApplicationPlugin;
class ScenarioPresenter;
namespace Command
{
class TriggerCommandFactoryList;
}
class SCORE_PLUGIN_SCENARIO_EXPORT EventActions final : public QObject
{
public:
  EventActions(ScenarioApplicationPlugin* parent);

  void makeGUIElements(score::GUIElements& ref);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

private:
  void addTriggerToTimeSync();
  void removeTriggerFromTimeSync();
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
