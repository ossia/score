#pragma once
#include <score/actions/Action.hpp>
#include <score/actions/Menu.hpp>
#include <score/selection/Selection.hpp>

class QAction;
class QMenu;
namespace Process
{
class LayerContextMenuManager;
}

namespace Scenario
{
class ScenarioApplicationPlugin;
class ScenarioPresenter;
}
namespace Engine
{
class ApplicationPlugin;
}

namespace Execution
{
class PlayContextMenu final : public QObject
{
public:
  PlayContextMenu(Engine::ApplicationPlugin& plug, const score::GUIApplicationContext& ctx);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

  void setEnabled(bool);

private:
  const score::GUIApplicationContext& m_ctx;

  QAction* m_recordAutomations{};
  QAction* m_recordMessages{};

  QAction* m_playStates{};
  QAction* m_playEvents{};
  QAction* m_playIntervals{};

  QAction* m_playFromHere{};
};
}
