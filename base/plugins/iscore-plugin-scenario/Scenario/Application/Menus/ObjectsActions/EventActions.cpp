#include "EventActions.hpp"

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>

#include <core/presenter/MenubarManager.hpp>
#include <core/document/Document.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <QAction>
#include <QMenu>

namespace Scenario
{
EventActions::EventActions(
        ScenarioApplicationPlugin* parent):
    m_parent{parent},
    m_triggerCommandFactory{parent->context.components.factory<Command::TriggerCommandFactoryList>()}
{
    using namespace iscore;
    m_addTrigger = new QAction{tr("Add Trigger"), this};
    m_addTrigger->setWhatsThis(MenuInterface::name(ContextMenu::Event));
    connect(m_addTrigger, &QAction::triggered,
            this, &EventActions::addTriggerToTimeNode);

    m_removeTrigger = new QAction{tr("Remove Trigger"), this};
    m_removeTrigger->setWhatsThis(MenuInterface::name(ContextMenu::Event));
    connect(m_removeTrigger, &QAction::triggered,
            this, &EventActions::removeTriggerFromTimeNode);
}

void EventActions::makeGUIElements(iscore::GUIElements& ref)
{
    using namespace iscore;
    auto& scenario_iface_cond = m_parent->context.actions.condition<Process::EnableWhenFocusedProcessIs<Scenario::ScenarioInterface>>();

    Menu& object = m_parent->context.menus.get().at(Menus::Object());
    object.menu()->addAction(m_addTrigger);
    object.menu()->addAction(m_removeTrigger);

    ref.actions.add<Actions::AddTrigger>(m_addTrigger);
    ref.actions.add<Actions::RemoveTrigger>(m_removeTrigger);

    scenario_iface_cond.add<Actions::AddTrigger>();
    scenario_iface_cond.add<Actions::RemoveTrigger>();
}


void EventActions::fillContextMenu(
        QMenu* menu,
        const Selection& sel,
        const TemporalScenarioPresenter& pres,
        const QPoint& p1,
        const QPointF& p2)
{
    fillContextMenu(menu, sel, p1, p2);
}

void EventActions::fillContextMenu(
        QMenu* menu,
        const Selection& sel,
        const QPoint&,
        const QPointF&)
{
    using namespace iscore;
    if(!sel.empty())
    {
        if(std::any_of(sel.cbegin(),
                       sel.cend(),
                       [] (const QObject* obj) { return dynamic_cast<const EventModel*>(obj); })) // TODO : event or timenode ?
        {
            auto tnSubmenu = menu->findChild<QMenu*>(MenuInterface::name(iscore::ContextMenu::Event));
            if(!tnSubmenu)
            {
                tnSubmenu = menu->addMenu(MenuInterface::name(iscore::ContextMenu::Event));
                tnSubmenu->setTitle(MenuInterface::name(iscore::ContextMenu::Event));
            }
            tnSubmenu->addAction(m_addTrigger);
            tnSubmenu->addAction(m_removeTrigger);
        }
    }
}


void EventActions::addTriggerToTimeNode()
{
    auto si = m_parent->focusedScenarioInterface();
    ISCORE_ASSERT(si);
    auto selectedTimeNodes = selectedElements(si->getTimeNodes());

    if(selectedTimeNodes.isEmpty())
    {
        // take tn from a selected event
        auto selectedEvents = selectedElements(si->getEvents());
        ISCORE_ASSERT(!selectedEvents.empty());
        // TODO maybe states, etc... ?

        auto ev = selectedEvents.first();
        auto& tn = Scenario::parentTimeNode(*ev, *si);
        selectedTimeNodes.append(&tn);
    }

    auto cmd = m_triggerCommandFactory.make(
                   &Scenario::Command::TriggerCommandFactory::make_addTriggerCommand,
                   **selectedTimeNodes.begin());

    if(cmd)
        emit dispatcher().submitCommand(cmd);
}

void EventActions::removeTriggerFromTimeNode()
{
    auto si = m_parent->focusedScenarioInterface();
    auto selectedTimeNodes = selectedElements(si->getTimeNodes());
    if(selectedTimeNodes.isEmpty())
    {
        auto selectedEvents = selectedElements(si->getEvents());
        ISCORE_ASSERT(!selectedEvents.empty());
        // TODO maybe states, etc... ?

        auto ev = selectedEvents.first();
        auto& tn = Scenario::parentTimeNode(*ev, *si);
        selectedTimeNodes.append(&tn);
    }

    auto cmd = m_triggerCommandFactory.make(
                   &Scenario::Command::TriggerCommandFactory::make_removeTriggerCommand,
                   **selectedTimeNodes.begin());

    if(cmd)
        emit dispatcher().submitCommand(cmd);
}

CommandDispatcher<> EventActions::dispatcher()
{
    CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
    return disp;
}
}
