#pragma once
#include <Scenario/Application/Menus/ObjectsActions/EventActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/IntervalActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/StateActions.hpp>

#include <score/actions/Action.hpp>
#include <score/actions/Menu.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/Selection.hpp>

namespace Scenario
{
struct Point;
class ScenarioApplicationPlugin;
class ScenarioDocumentModel;
class ScenarioDocumentPresenter;
class ScenarioPresenter;
class SCORE_PLUGIN_SCENARIO_EXPORT ObjectMenuActions final : public QObject
{
public:
  ObjectMenuActions(ScenarioApplicationPlugin* parent);

  void makeGUIElements(score::GUIElements& ref);
  void setupContextMenu(Process::LayerContextMenuManager& ctxm);

  CommandDispatcher<> dispatcher() const;

  auto appPlugin() const { return m_parent; }

private:
  void copySelectedElementsToJson(JSONReader& r);
  void cutSelectedElementsToJson(JSONReader& r);
  void pasteElements(const rapidjson::Value& obj, const Scenario::Point& origin);
  void pasteElementsAfter(
      const rapidjson::Value& obj,
      const Scenario::Point& origin,
      const Selection& sel);
  void writeJsonToSelectedElements(const rapidjson::Value& obj);

  ScenarioDocumentModel* getScenarioDocModel() const;
  ScenarioDocumentPresenter* getScenarioDocPresenter() const;
  ScenarioApplicationPlugin* m_parent{};

  EventActions m_eventActions;
  IntervalActions m_cstrActions;
  StateActions m_stateActions;

  QAction* m_removeElements{};
  QAction* m_copyContent{};
  QAction* m_cutContent{};
  QAction* m_pasteElements{};
  QAction* m_pasteElementsAfter{};
  QAction* m_elementsToJson{};
  QAction* m_mergeTimeSyncs{};
  QAction* m_mergeEvents{};
  QAction* m_encapsulate{};
  QAction* m_decapsulate{};
  QAction* m_duplicate{};

  QAction* m_selectAll{};
  QAction* m_deselectAll{};
  QAction* m_selectTop{};
  QAction* m_goToParent{};
};
}
