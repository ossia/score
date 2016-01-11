#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <Process/ProcessFactoryKey.hpp>

class AddProcessDialog;
namespace Scenario
{
class ConstraintActions final : public ScenarioActions
{
    public:
    ConstraintActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);
    void fillMenuBar(iscore::MenubarManager *menu) override;
    void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;
    void setEnabled(bool) override;
    bool populateToolBar(QToolBar*) override;

    QList<QAction*> actions() const override;

    private:
    void addProcessInConstraint(const ProcessFactoryKey&);

    CommandDispatcher<> dispatcher();

    QAction *m_addProcess{};
    QAction *m_interp{};

    AddProcessDialog* m_addProcessDialog{};

};
}
