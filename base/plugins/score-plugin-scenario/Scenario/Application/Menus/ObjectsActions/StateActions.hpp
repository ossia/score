#pragma once

#include <Process/Layer/LayerContextMenu.hpp>

#include <score/actions/Menu.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QAction>

#include <score_plugin_scenario_export.h>
namespace score
{
struct GUIElements;
}
namespace Scenario
{
class ScenarioApplicationPlugin;
class ScenarioPresenter;
class SCORE_PLUGIN_SCENARIO_EXPORT StateActions final : public QObject
{
public:
  StateActions(ScenarioApplicationPlugin* parent);

  void makeGUIElements(score::GUIElements& ref);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

private:
  CommandDispatcher<> dispatcher();

  ScenarioApplicationPlugin* m_parent{};
  QAction* m_refreshStates{};
  QAction* m_snapshot{};
};
}
