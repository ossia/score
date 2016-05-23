#pragma once
#include <QJsonObject>
#include <QList>
#include <QPoint>


#include "ScenarioActions.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>
#include <Scenario/Application/Menus/ObjectsActions/EventActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/ConstraintActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/StateActions.hpp>
class QAction;
class QMenu;
class QToolBar;

namespace iscore {
class MenubarManager;
}  // namespace iscore

namespace Scenario
{
struct Point;
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class ISCORE_PLUGIN_SCENARIO_EXPORT ObjectMenuActions : public QObject
{
    public:
        ObjectMenuActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu);
        void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&);
        void setEnabled(bool);
        bool populateToolBar(QToolBar*);


        QList<QAction*> actions() const;
        CommandDispatcher<> dispatcher() const;

        const EventActions* eventActions() const
        { return &m_eventActions; }
        const ConstraintActions* constraintActions() const
        { return &m_cstrActions; }
        const StateActions* stateActions() const
        { return &m_stateActions; }

        QAction* clearContent()
        { return m_clearElements; }
        QAction* copyContent()
        { return m_copyContent; }
        QAction* pasteContent()
        { return m_pasteContent; }
        QAction* elementsToJson()
        { return m_elementsToJson; }

        auto appPlugin() const
        { return m_parent; }
    private:
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void pasteElements(const QJsonObject& obj, const Scenario::Point& origin);
        void writeJsonToSelectedElements(const QJsonObject &obj);

        iscore::ToplevelMenuElement m_menuElt;
        ScenarioApplicationPlugin* m_parent{};

        EventActions m_eventActions;
        ConstraintActions m_cstrActions;
        StateActions m_stateActions;

        QAction* m_removeElements{};
        QAction *m_clearElements{};
        QAction *m_copyContent{};
        QAction *m_cutContent{};
        QAction *m_pasteContent{};
        QAction *m_elementsToJson{};
};
}
