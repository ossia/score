#pragma once

#include "Control/Menus/AbstractMenuActions.hpp"

class ToolMenuActions : public AbstractMenuActions
{
    public:
        ToolMenuActions(iscore::ToplevelMenuElement menuElt, ScenarioControl* parent);
        void fillMenuBar(iscore::MenubarManager *menu);
        void fillContextMenu(QMenu* menu);
        void makeToolBar(QToolBar* bar);
        void setEnabled(bool);

        QList<QAction*> actions();
        QList<QAction*> modeActions();
        QList<QAction*> toolActions();
        QAction* shiftAction();


    private:
        QActionGroup* m_scenarioScaleModeActionGroup{};
        QActionGroup* m_scenarioToolActionGroup{};
        QAction* m_shiftAction{};

        QAction* m_selecttool{};
};
