#pragma once


#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <QAction>
#include <QToolBar>
#include <iscore/actions/Action.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Process
{
class LayerContextMenuManager;
class ProcessModelFactory; class LayerFactory;
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
        void setupContextMenu(Process::LayerContextMenuManager& ctxm);

    private:
        void addProcessInConstraint(const UuidKey<Process::ProcessModelFactory>&);

        CommandDispatcher<> dispatcher();

        ScenarioApplicationPlugin* m_parent{};
        QAction *m_addProcess{};
        QAction *m_interp{};
        QAction* m_curves{};

};
}
