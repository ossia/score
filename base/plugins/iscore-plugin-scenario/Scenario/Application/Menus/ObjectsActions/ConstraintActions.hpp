#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <QAction>
#include <QToolBar>
#include <iscore/actions/Action.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Process
{
class ProcessFactory;
}
namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class ConstraintViewModel;
class AddProcessDialog;
class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintActions : public QObject
{
    public:
    ConstraintActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);
    ~ConstraintActions();

    void makeGUIElements(iscore::GUIElements& ref);
    void fillMenuBar(iscore::MenubarManager *menu) ;
    void fillContextMenu(
            QMenu* menu,
            const Selection&,
            const TemporalScenarioPresenter& pres,
            const QPoint&,
            const QPointF&) ;
    void fillContextMenu(
            QMenu* menu,
            const Selection&,
            const ConstraintViewModel& vm,
            const QPoint&,
            const QPointF&);
    void setEnabled(bool) ;
    bool populateToolBar(QToolBar*) ;

    QList<QAction*> actions() const ;

    QAction* addProcess() const
    { return m_addProcess; }
    QAction* interpolateStates() const
    { return m_interp; }
    private:
    void addProcessInConstraint(const UuidKey<Process::ProcessFactory>&);

    CommandDispatcher<> dispatcher();

    iscore::ToplevelMenuElement m_menuElt;
    ScenarioApplicationPlugin* m_parent{};
    QAction *m_addProcess{};
    QAction *m_interp{};

    AddProcessDialog* m_addProcessDialog{};

};
}
