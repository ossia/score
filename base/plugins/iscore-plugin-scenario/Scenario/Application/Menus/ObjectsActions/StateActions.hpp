#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

namespace Scenario
{
class StateActions final : public ScenarioActions
{
    public:
    StateActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);
    void fillMenuBar(iscore::MenubarManager *menu) override;
    void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;
    void setEnabled(bool) override;

    QList<QAction*> actions() const override;

    QAction* updateStates() const
    { return m_updateStates; }
    private:
    CommandDispatcher<> dispatcher();

    QAction* m_updateStates{};
};
}
