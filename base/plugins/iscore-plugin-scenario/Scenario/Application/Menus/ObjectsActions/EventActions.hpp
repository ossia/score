#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
namespace Scenario
{
class EventActions final : public ScenarioActions
{
    public:
        EventActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;
        void setEnabled(bool) override;

        QList<QAction*> actions() const override;

    private:
        void addTriggerToTimeNode();
        void removeTriggerFromTimeNode();

        CommandDispatcher<> dispatcher();

        QAction *m_addTrigger{};
        QAction *m_removeTrigger{};

};
}
