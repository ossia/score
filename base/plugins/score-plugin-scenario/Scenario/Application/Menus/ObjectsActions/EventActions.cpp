// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "EventActions.hpp"

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <Scenario/Commands/TimeSync/AddTrigger.hpp>
#include <Scenario/Commands/TimeSync/RemoveTrigger.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <core/document/Document.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/widgets/SetIcons.hpp>
#include <Process/ProcessContext.hpp>
#include <QAction>
#include <QMenu>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Commands/Event/SetCondition.hpp>
#include <core/application/ApplicationSettings.hpp>
namespace Scenario
{
EventActions::EventActions(ScenarioApplicationPlugin* parent)
    : m_parent{parent}
    , m_triggerCommandFactory{
          parent->context
              .interfaces<Command::TriggerCommandFactoryList>()}
{
  if(!parent->context.applicationSettings.gui)
    return;
  using namespace score;

  /// Add Trigger ///
  m_addTrigger = new QAction{tr("Enable trigger"), this};
  connect(
      m_addTrigger, &QAction::triggered, this,
      &EventActions::addTriggerToTimeSync);
  m_addTrigger->setEnabled(false);

  m_addTrigger->setToolTip(tr("Enable trigger"));
  setIcons(m_addTrigger, ":/icons/trigger_on.png", ":/icons/trigger_off.png");

  /// Remove Trigger ///
  m_removeTrigger = new QAction{tr("Disable trigger"), this};
  connect(
      m_removeTrigger, &QAction::triggered, this,
      &EventActions::removeTriggerFromTimeSync);
  m_removeTrigger->setEnabled(false);

  /// Add Condition ///
  m_addCondition = new QAction{tr("Add Condition"), this};
  connect(
      m_addCondition, &QAction::triggered, this,
      &EventActions::addCondition);
  m_addCondition->setEnabled(false);

  m_addCondition->setToolTip(tr("Add Condition"));
  setIcons(m_addCondition, ":/icons/condition_on.png", ":/icons/condition_off.png");

  /// Remove Condition ///
  m_removeCondition = new QAction{tr("Remove Condition"), this};
  connect(
      m_removeCondition, &QAction::triggered, this,
      &EventActions::removeCondition);
  m_removeCondition->setEnabled(false);

}

void EventActions::makeGUIElements(score::GUIElements& ref)
{
  using namespace score;

  Menu& object = m_parent->context.menus.get().at(Menus::Object());
  object.menu()->addAction(m_addTrigger);
  object.menu()->addAction(m_removeTrigger);
  object.menu()->addAction(m_addCondition);
  object.menu()->addAction(m_removeCondition);

  auto bar = new QToolBar{tr("Event")};
  bar->addAction(m_addTrigger);
  bar->addAction(m_addCondition);
  ref.toolbars.emplace_back(bar, StringKey<score::Toolbar>("Event"), 0, 1);

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
        using namespace score;
        auto sel = ctx.context.selectionStack.currentSelection();
        if (sel.empty())
          return;

        if (std::any_of(sel.cbegin(), sel.cend(), [](const QObject* obj) {
              return dynamic_cast<const EventModel*>(obj);
            })) // TODO : event or timesync ?
        {
          auto m = menu.addMenu(tr("Event"));

          m->addAction(m_addTrigger);
          m->addAction(m_removeTrigger);
        }
      });

  ctxm.insert(std::move(cm));
}

void EventActions::addTriggerToTimeSync()
{
  auto si = focusedScenarioInterface(m_parent->currentDocument()->context());
  if(!si)
    return;

  auto selectedTimeSyncs = selectedElements(si->getTimeSyncs());

  if (selectedTimeSyncs.isEmpty())
  {
    // take tn from a selected event
    auto selectedEvents = selectedElements(si->getEvents());
    if(selectedEvents.empty())
    {
      auto selectedStates = selectedElements(si->getStates());
      if(!selectedStates.empty())
      {
        auto& tn = Scenario::parentTimeSync(*selectedStates.first(), *si);
        selectedTimeSyncs.append(&tn);
      }
      else
      {
        return;
      }
    }
    else
    {
      auto ev = selectedEvents.first();
      auto& tn = Scenario::parentTimeSync(*ev, *si);
      selectedTimeSyncs.append(&tn);
    }
  }

  selectedTimeSyncs = selectedTimeSyncs.toSet().toList();

  auto cmd = m_triggerCommandFactory.make(
      &Scenario::Command::TriggerCommandFactory::make_addTriggerCommand,
      **selectedTimeSyncs.begin());

  if (cmd)
    dispatcher().submitCommand(cmd);
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
    dispatcher().submitCommand(cmd);
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
    dispatcher().submitCommand(cmd);
  }
}

void EventActions::removeTriggerFromTimeSync()
{
  auto si = focusedScenarioInterface(m_parent->currentDocument()->context());
  auto selectedTimeSyncs = selectedElements(si->getTimeSyncs());
  if (selectedTimeSyncs.isEmpty())
  {
    auto selectedEvents = selectedElements(si->getEvents());
    SCORE_ASSERT(!selectedEvents.empty());
    // TODO maybe states, etc... ?

    auto ev = selectedEvents.first();
    auto& tn = Scenario::parentTimeSync(*ev, *si);
    selectedTimeSyncs.append(&tn);
  }

  auto cmd = m_triggerCommandFactory.make(
      &Scenario::Command::TriggerCommandFactory::make_removeTriggerCommand,
      **selectedTimeSyncs.begin());

  if (cmd)
    dispatcher().submitCommand(cmd);
}

CommandDispatcher<> EventActions::dispatcher()
{
  CommandDispatcher<> disp{
      m_parent->currentDocument()->context().commandStack};
  return disp;
}
}
