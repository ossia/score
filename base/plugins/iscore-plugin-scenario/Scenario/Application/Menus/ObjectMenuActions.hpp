#pragma once
#include <QJsonObject>
#include <QList>
#include <QPoint>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>
#include <Scenario/Application/Menus/ObjectsActions/EventActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/ConstraintActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/StateActions.hpp>
#include <iscore/actions/Action.hpp>
class QAction;
class QMenu;
class QToolBar;

namespace Scenario
{
struct Point;
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class ISCORE_PLUGIN_SCENARIO_EXPORT ObjectMenuActions : public QObject
{
    public:
        ObjectMenuActions(ScenarioApplicationPlugin* parent);

        void makeGUIElements(iscore::GUIElements& ref);
        void setupContextMenu(Process::LayerContextMenuManager& ctxm);

        CommandDispatcher<> dispatcher() const;

        auto appPlugin() const
        { return m_parent; }
    private:
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void pasteElements(const QJsonObject& obj, const Scenario::Point& origin);
        void writeJsonToSelectedElements(const QJsonObject &obj);

        ScenarioApplicationPlugin* m_parent{};

        EventActions m_eventActions;
        ConstraintActions m_cstrActions;
        StateActions m_stateActions;

        QAction *m_removeElements{};
        QAction *m_clearElements{};
        QAction *m_copyContent{};
        QAction *m_cutContent{};
        QAction *m_pasteContent{};
        QAction *m_elementsToJson{};
};
}
