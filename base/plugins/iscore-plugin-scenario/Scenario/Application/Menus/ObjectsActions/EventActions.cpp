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
    connect(m_addTrigger, &QAction::triggered,
            this, &EventActions::addTriggerToTimeNode);

    m_removeTrigger = new QAction{tr("Remove Trigger"), this};
    connect(m_removeTrigger, &QAction::triggered,
            this, &EventActions::removeTriggerFromTimeNode);
}

void EventActions::makeGUIElements(iscore::GUIElements& ref)
{
    using namespace iscore;

    Menu& object = m_parent->context.menus.get().at(Menus::Object());
    object.menu()->addAction(m_addTrigger);
    object.menu()->addAction(m_removeTrigger);

    ref.actions.add<Actions::AddTrigger>(m_addTrigger);
    ref.actions.add<Actions::RemoveTrigger>(m_removeTrigger);

    auto& cond = m_parent->context.actions.condition<iscore::EnableWhenSelectionContains<Scenario::EventModel>>();
    cond.add<Actions::AddTrigger>();
    cond.add<Actions::RemoveTrigger>();
}

void EventActions::setupContextMenu(Process::LayerContextMenuManager &ctxm)
{
    using namespace Process;
    Process::LayerContextMenu cm = MetaContextMenu<ContextMenus::EventContextMenu>::make();

    cm.functions.push_back(
                [this] (QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx)
    {
        using namespace iscore;
        auto sel = ctx.context.selectionStack.currentSelection();
        if(sel.empty())
            return;

        if(std::any_of(sel.cbegin(),
                       sel.cend(),
                       [] (const QObject* obj) { return dynamic_cast<const EventModel*>(obj); })) // TODO : event or timenode ?
        {
            auto m = menu.addMenu(tr("Event"));

            m->addAction(m_addTrigger);
            m->addAction(m_removeTrigger);
        }
    });

    ctxm.insert(std::move(cm));
}

void EventActions::addTriggerToTimeNode()
{
    auto si = focusedScenarioInterface(m_parent->currentDocument()->context());
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
    auto si = focusedScenarioInterface(m_parent->currentDocument()->context());
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
