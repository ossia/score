#include "EventActions.hpp"

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <core/presenter/MenubarManager.hpp>
#include <core/document/Document.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

#include <QAction>
#include <QMenu>

namespace Scenario
{
EventActions::EventActions(iscore::ToplevelMenuElement menuElt,
               ScenarioApplicationPlugin* parent):
     ScenarioActions(menuElt, parent)
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


void EventActions::fillMenuBar(iscore::MenubarManager* menu)
{
    menu->insertActionIntoToplevelMenu(m_menuElt, m_addTrigger);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_removeTrigger);
}

void EventActions::fillContextMenu(
    QMenu* menu,
    const Selection& sel,
    const TemporalScenarioPresenter& pres,
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

void EventActions::setEnabled(bool b)
{
    for (auto& act : actions())
    {
    act->setEnabled(b);
    }
}

QList<QAction*> EventActions::actions() const
{
    QList<QAction*> lst{
    m_addTrigger,
    m_removeTrigger
    };
    return lst;
}

void EventActions::addTriggerToTimeNode()
{
    auto selectedTimeNodes = selectedElements(m_parent->focusedScenarioModel()->timeNodes);

    if(selectedTimeNodes.isEmpty())
    {
        // take tn from a selected event
        auto selectedEvents = selectedElements(m_parent->focusedScenarioModel()->events);
        auto ev = selectedEvents.first();
        auto scenar = static_cast<Scenario::ScenarioModel*>(ev->parent());
        auto& tn = Scenario::parentTimeNode(*ev, *scenar);
        selectedTimeNodes.append(&tn);
    }

    auto cmd = new Scenario::Command::AddTrigger<Scenario::ScenarioModel>{**selectedTimeNodes.begin()};
    emit dispatcher().submitCommand(cmd);
}

void EventActions::removeTriggerFromTimeNode()
{
    auto selectedTimeNodes = selectedElements(m_parent->focusedScenarioModel()->timeNodes);
    if(selectedTimeNodes.isEmpty())
    {
        auto selectedEvents = selectedElements(m_parent->focusedScenarioModel()->events);
        auto ev = selectedEvents.first();
        auto scenar = static_cast<Scenario::ScenarioModel*>(ev->parent());
        auto& tn = Scenario::parentTimeNode(*ev, *scenar);
        selectedTimeNodes.append(&tn);
    }

    auto cmd = new Scenario::Command::RemoveTrigger<Scenario::ScenarioModel>{**selectedTimeNodes.begin()};
    emit dispatcher().submitCommand(cmd);
}

CommandDispatcher<> EventActions::dispatcher()
{
    CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
    return disp;
}
}
