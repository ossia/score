#pragma once
#include <QJsonObject>
#include <QList>
#include <QPoint>

#include <Scenario/Application/Menus/ObjectsActions/IntervalActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/EventActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/StateActions.hpp>
#include <iscore/actions/Action.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/actions/Menu.hpp>
#include <iscore/selection/Selection.hpp>
class QAction;
class QMenu;
class QToolBar;

namespace Scenario
{
struct Point;
class ScenarioApplicationPlugin;
class ScenarioDocumentModel;
class ScenarioDocumentPresenter;
class TemporalScenarioPresenter;
class ISCORE_PLUGIN_SCENARIO_EXPORT ObjectMenuActions : public QObject
{
public:
  ObjectMenuActions(ScenarioApplicationPlugin* parent);

  void makeGUIElements(iscore::GUIElements& ref);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

  CommandDispatcher<> dispatcher() const;

  auto appPlugin() const
  {
    return m_parent;
  }

private:
  QJsonObject copySelectedElementsToJson();
  QJsonObject cutSelectedElementsToJson();
  void pasteElements(const QJsonObject& obj, const Scenario::Point& origin);
  void writeJsonToSelectedElements(const QJsonObject& obj);

  ScenarioDocumentModel* getScenarioDocModel() const;
  ScenarioDocumentPresenter* getScenarioDocPresenter() const;
  ScenarioApplicationPlugin* m_parent{};

  EventActions m_eventActions;
  IntervalActions m_cstrActions;
  StateActions m_stateActions;

  QAction* m_removeElements{};
  QAction* m_clearElements{};
  QAction* m_copyContent{};
  QAction* m_cutContent{};
  QAction* m_pasteContent{};
  QAction* m_pasteElements{};
  QAction* m_elementsToJson{};
  QAction* m_mergeTimeSyncs{};


  QAction* m_selectAll{};
  QAction* m_deselectAll{};
  QAction* m_selectTop{};
};
}
