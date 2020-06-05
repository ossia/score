#pragma once

#include <score/actions/Action.hpp>
#include <score/actions/Menu.hpp>
#include <score/selection/Selection.hpp>
#include <Process/ExpandMode.hpp>

namespace score
{
struct GUIElements;
}
class QAction;
class QActionGroup;
class QMenu;
class QToolBar;
namespace Scenario
{
class ScenarioApplicationPlugin;
class ScenarioPresenter;
class ToolMenuActions final : public QObject
{
public:
  ToolMenuActions(ScenarioApplicationPlugin* parent);

  void makeGUIElements(score::GUIElements& ref);

private:
  void keyPressed(int key);
  void keyReleased(int key);

  void setExpandMode(ExpandMode mode);

  ScenarioApplicationPlugin* m_parent{};

  QActionGroup* m_scenarioToolActionGroup{};

  QAction* m_selecttool{};
  QAction* m_createtool{};
  QAction* m_playtool{};

  QAction* m_scaleAction{};
  QAction* m_lockAction{};
};
}
