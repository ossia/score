#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore_plugin_scenario_export.h>
#include <iscore/actions/Action.hpp>
namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;

namespace Command
{
class TriggerCommandFactoryList;
}
class ISCORE_PLUGIN_SCENARIO_EXPORT EventActions : public QObject
{
    public:
        EventActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);

        void makeGUIElements(iscore::GUIElementsRef ref);

        void fillMenuBar(iscore::MenubarManager *menu) ;
        void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) ;
        void fillContextMenu(QMenu* menu, const Selection&, const QPoint&, const QPointF&);
        void setEnabled(bool) ;

        QList<QAction*> actions() const ;

        QAction* addTrigger() const
        { return m_addTrigger; }
        QAction* removeTrigger() const
        { return m_removeTrigger; }

    private:
        void addTriggerToTimeNode();
        void removeTriggerFromTimeNode();

        CommandDispatcher<> dispatcher();

        iscore::ToplevelMenuElement m_menuElt;
        ScenarioApplicationPlugin* m_parent{};
        QAction *m_addTrigger{};
        QAction *m_removeTrigger{};

        const Command::TriggerCommandFactoryList& m_triggerCommandFactory;
};
}
