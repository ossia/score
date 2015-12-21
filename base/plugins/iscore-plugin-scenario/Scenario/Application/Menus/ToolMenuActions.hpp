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
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
namespace iscore {
class MenubarManager;
}  // namespace iscore

class ToolMenuActions final : public ScenarioActions
{
    public:
        ToolMenuActions(iscore::ToplevelMenuElement menuElt, ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(
                QMenu* menu,
                const Selection&sel,
                const TemporalScenarioPresenter& pres,
                const QPoint&,
                const QPointF&) override;
        bool populateToolBar(QToolBar* bar) override;
        void setEnabled(bool) override;

        QList<QAction*> modeActions();
        QList<QAction*> toolActions();
        QAction* shiftAction();

    private:
        void keyPressed(int key);
        void keyReleased(int key);

        QActionGroup* m_scenarioScaleModeActionGroup{};
        QActionGroup* m_scenarioToolActionGroup{};
        QAction* m_shiftAction{};

        QAction* m_selecttool{};
        QAction* m_createtool{};
};
