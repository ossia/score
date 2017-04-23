#include "EventActions.hpp"

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <Scenario/Commands/TimeNode/AddTrigger.hpp>
#include <Scenario/Commands/TimeNode/RemoveTrigger.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <core/document/Document.hpp>
#include <iscore/actions/ActionManager.hpp>
#include <iscore/actions/MenuManager.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/widgets/SetIcons.hpp>
#include <Process/ProcessContext.hpp>
#include <QAction>
#include <QMenu>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Event/SetCondition.hpp>

namespace Scenario
{
EventActions::EventActions(ScenarioApplicationPlugin* parent)
    : m_parent{parent}
    , m_triggerCommandFactory{
          parent->context
              .interfaces<Command::TriggerCommandFactoryList>()}
{
  using namespace iscore;

  /// Add Trigger ///
  m_addTrigger = new QAction{tr("Add Trigger"), this};
  connect(
      m_addTrigger, &QAction::triggered, this,
      &EventActions::addTriggerToTimeNode);
  m_addTrigger->setEnabled(false);

  m_addTrigger->setToolTip(tr("Add trigger"));
  setIcons(m_addTrigger, ":/icons/trigger_on.png", ":/icons/trigger_off.png");

  /// Remove Trigger ///
  m_removeTrigger = new QAction{tr("Remove Trigger"), this};
  connect(
      m_removeTrigger, &QAction::triggered, this,
      &EventActions::removeTriggerFromTimeNode);
  m_removeTrigger->setEnabled(false);

  // Add Condition ///
  m_addCondition = new QAction{tr("Add Condition"), this};
  connect(
      m_addCondition, &QAction::triggered, this,
      &EventActions::addCondition);
  m_addCondition->setEnabled(false);

  m_addCondition->setToolTip(tr("Add Condition"));
  setIcons(m_addCondition, ":/icons/condition_on.png", ":/icons/condition_off.png");

  // Remove Condition ///
  m_removeCondition = new QAction{tr("Remove Condition"), this};
  connect(
      m_removeCondition, &QAction::triggered, this,
      &EventActions::removeCondition);
  m_removeCondition->setEnabled(false);

}

void EventActions::makeGUIElements(iscore::GUIElements& ref)
{
  using namespace iscore;

  Menu& object = m_parent->context.menus.get().at(Menus::Object());
  object.menu()->addAction(m_addTrigger);
  object.menu()->addAction(m_removeTrigger);
  object.menu()->addAction(m_addCondition);
  object.menu()->addAction(m_removeCondition);

  auto bar = new QToolBar{tr("Event")};
  bar->addAction(m_addTrigger);
  bar->addAction(m_addCondition);
  ref.toolbars.emplace_back(bar, StringKey<iscore::Toolbar>("Event"), 0, 1);

  ref.actions.add<Actions::AddTrigger>(m_addTrigger);
  ref.actions.add<Actions::RemoveTrigger>(m_removeTrigger);
  ref.actions.add<Actions::AddCondition>(m_addCondition);
  ref.actions.add<Actions::RemoveCondition>(m_removeCondition);

  auto& cond
      = m_parent->context.actions
            .condition<EnableWhenScenarioInterfaceInstantObject>();
  cond.add<Actions::AddTrigger>();
  cond.add<Actions::RemoveTrigger>();
  cond.add<Actions::AddCondition>();
  cond.add<Actions::RemoveCondition>();
}

void EventActions::setupContextMenu(Process::LayerContextMenuManager& ctxm)
{
  using namespace Process;
  Process::LayerContextMenu cm
      = MetaContextMenu<ContextMenus::EventContextMenu>::make();

  cm.functions.push_back(
      [this](QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx) {
        using namespace iscore;
        auto sel = ctx.context.selectionStack.currentSelection();
        if (sel.empty())
          return;

        if (std::any_of(sel.cbegin(), sel.cend(), [](const QObject* obj) {
              return dynamic_cast<const EventModel*>(obj);
            })) // TODO : event or timenode ?
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
  if(!si)
    return;

  auto selectedTimeNodes = selectedElements(si->getTimeNodes());

  if (selectedTimeNodes.isEmpty())
  {
    // take tn from a selected event
    auto selectedEvents = selectedElements(si->getEvents());
    if(selectedEvents.empty())
    {
      auto selectedStates = selectedElements(si->getStates());
      if(!selectedStates.empty())
      {
        auto& tn = Scenario::parentTimeNode(*selectedStates.first(), *si);
        selectedTimeNodes.append(&tn);
      }
      else
      {
        return;
      }
    }
    else
    {
      auto ev = selectedEvents.first();
      auto& tn = Scenario::parentTimeNode(*ev, *si);
      selectedTimeNodes.append(&tn);
    }
  }

  selectedTimeNodes = selectedTimeNodes.toSet().toList();

  auto cmd = m_triggerCommandFactory.make(
      &Scenario::Command::TriggerCommandFactory::make_addTriggerCommand,
      **selectedTimeNodes.begin());

  if (cmd)
    emit dispatcher().submitCommand(cmd);
}

void EventActions::addCondition()
{
  auto si = focusedScenarioInterface(m_parent->currentDocument()->context());
  if(!si)
    return;

  auto selectedEvents = selectedElements(si->getEvents());
  if(selectedEvents.empty())
  {
    auto selectedStates = selectedElements(si->getStates());
    if(!selectedStates.empty())
    {
      auto& ev = Scenario::parentEvent(*selectedStates.first(), *si);
      selectedEvents.append(&ev);
    }
    else
    {
      return;
    }
  }

  const EventModel& ev = *selectedEvents.first();
  if(ev.condition() == State::Expression{})
  {
    auto cmd = new Scenario::Command::SetCondition{ev, State::defaultTrueExpression()};
    emit dispatcher().submitCommand(cmd);
  }
}

void EventActions::removeCondition()
{
  auto si = focusedScenarioInterface(m_parent->currentDocument()->context());
  if(!si)
    return;

  auto selectedEvents = selectedElements(si->getEvents());
  if(selectedEvents.empty())
  {
    auto selectedStates = selectedElements(si->getStates());
    if(!selectedStates.empty())
    {
      auto& ev = Scenario::parentEvent(*selectedStates.first(), *si);
      selectedEvents.append(&ev);
    }
    else
    {
      return;
    }
  }

  const EventModel& ev = *selectedEvents.first();
  if(ev.condition() != State::Expression{})
  {
    auto cmd = new Scenario::Command::SetCondition{ev, State::Expression{}};
    emit dispatcher().submitCommand(cmd);
  }
}

void EventActions::removeTriggerFromTimeNode()
{
  auto si = focusedScenarioInterface(m_parent->currentDocument()->context());
  auto selectedTimeNodes = selectedElements(si->getTimeNodes());
  if (selectedTimeNodes.isEmpty())
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

  if (cmd)
    emit dispatcher().submitCommand(cmd);
}

CommandDispatcher<> EventActions::dispatcher()
{
  CommandDispatcher<> disp{
      m_parent->currentDocument()->context().commandStack};
  return disp;
}
}
