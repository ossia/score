#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

namespace Process
{
class ProcessFactory;
}
namespace Scenario
{
class AddProcessDialog;
class ConstraintActions final : public ScenarioActions
{
    public:
    ConstraintActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);
    ~ConstraintActions();
    void fillMenuBar(iscore::MenubarManager *menu) override;
    void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;
    void setEnabled(bool) override;
    bool populateToolBar(QToolBar*) override;

    QList<QAction*> actions() const override;

    private:
    void addProcessInConstraint(const UuidKey<Process::ProcessFactory>&);

    CommandDispatcher<> dispatcher();

    QAction *m_addProcess{};
    QAction *m_interp{};

    AddProcessDialog* m_addProcessDialog{};

};
}
