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
        ConstraintActions(ScenarioApplicationPlugin* parent);
        ~ConstraintActions();

        void makeGUIElements(iscore::GUIElements& ref);
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

    private:
        void addProcessInConstraint(const UuidKey<Process::ProcessFactory>&);

        CommandDispatcher<> dispatcher();

        ScenarioApplicationPlugin* m_parent{};
        QAction *m_addProcess{};
        QAction *m_interp{};

        AddProcessDialog* m_addProcessDialog{};

};
}
