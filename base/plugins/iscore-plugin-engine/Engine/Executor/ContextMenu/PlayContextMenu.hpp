#pragma once
#include <QPoint>

#include <iscore/actions/Action.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>
class QAction;
class QMenu;
namespace Process
{
class LayerContextMenuManager;
}

namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
}
namespace Engine
{
class ApplicationPlugin;
namespace Execution
{
class PlayContextMenu final : public QObject
{
public:
  PlayContextMenu(
      ApplicationPlugin& plug, const iscore::ApplicationContext& ctx);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

  void setEnabled(bool);

private:
  const iscore::ApplicationContext& m_ctx;

  QAction* m_recordAutomations{};
  QAction* m_recordMessages{};

  QAction* m_playStates{};
  QAction* m_playEvents{};
  QAction* m_playConstraints{};

  QAction* m_playFromHere{};
};
}
}
