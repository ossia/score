#pragma once
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <QList>
#include <QPoint>

#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>

class QAction;
class QActionGroup;
class QMenu;
class QToolBar;
namespace iscore {
class MenubarManager;
}  // namespace iscore

namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class ToolMenuActions : public QObject
{
    public:
        ToolMenuActions(iscore::ToplevelMenuElement menuElt, ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu);
        void fillContextMenu(
                QMenu* menu,
                const Selection&sel,
                const TemporalScenarioPresenter& pres,
                const QPoint&,
                const QPointF&);
        bool populateToolBar(QToolBar* bar);
        void setEnabled(bool);

        QList<QAction*> modeActions();
        QList<QAction*> toolActions();
        QAction* shiftAction();

    private:
        void keyPressed(int key);
        void keyReleased(int key);

        ScenarioApplicationPlugin* m_parent{};
        iscore::ToplevelMenuElement m_menuElt;

        QActionGroup* m_scenarioScaleModeActionGroup{};
        QActionGroup* m_scenarioToolActionGroup{};
        QAction* m_shiftAction{};

        QAction* m_selecttool{};
        QAction* m_createtool{};
        QAction* m_playtool{};
};
}
