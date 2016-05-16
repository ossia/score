#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT EventActions final : public ScenarioActions
{
    public:
        EventActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;
        void fillContextMenu(QMenu* menu, const Selection&, const QPoint&, const QPointF&);
        void setEnabled(bool) override;

        QList<QAction*> actions() const override;

        QAction* addTrigger() const
        { return m_addTrigger; }
        QAction* removeTrigger() const
        { return m_removeTrigger; }

    private:
        void addTriggerToTimeNode();
        void removeTriggerFromTimeNode();

        CommandDispatcher<> dispatcher();

        QAction *m_addTrigger{};
        QAction *m_removeTrigger{};

};
}
