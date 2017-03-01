#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_constraint.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_node.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Engine/iscore2OSSIA.hpp>
#include <QDebug>
#include <algorithm>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <vector>

#include "ScenarioComponent.hpp"
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/EventComponent.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/Executor/StateComponent.hpp>
#include <Engine/Executor/TimeNodeComponent.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Scenario/ExecutionChecker/CSPCoherencyCheckerInterface.hpp>
#include <Scenario/ExecutionChecker/CSPCoherencyCheckerList.hpp>
#include <Scenario/ExecutionChecker/CoherencyCheckerFactoryInterface.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/IdentifiedObjectMap.hpp>
#include <iscore/model/Identifier.hpp>

namespace Process
{
class ProcessModel;
}
class QObject;
namespace ossia
{
class time_process;
} // namespace OSSIA

namespace Engine
{
namespace Execution
{
ScenarioComponentBase::ScenarioComponentBase(
    ConstraintComponent& parentConstraint,
    Scenario::ProcessModel& element,
    const Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
  : ProcessComponent_T<Scenario::ProcessModel, ossia::scenario>{parentConstraint,
                                                                element, ctx,
                                                                id,
                                                                "ScenarioComponent",
                                                                nullptr}
  , m_ctx{ctx}
{
  this->setObjectName("OSSIAScenarioElement");

  // Setup of the OSSIA API Part
  m_ossia_process = std::make_shared<ossia::scenario>();

  // Note : the hierarchical scenario shall create the time nodes first.
  // A better way would be :
  // * Either to not have a dependency ordering, which would require two passes
  // * Or to have the HierarchicalScenario take a variadic amount of stuff and init them in the right order.
}

ScenarioComponent::ScenarioComponent(
    ConstraintComponent& cst,
    Scenario::ProcessModel& proc,
    const Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent):
  ScenarioComponentHierarchy{iscore::lazy_init_t{}, cst, proc, ctx, id, parent}
{
}

void ScenarioComponent::init()
{
  ScenarioComponentHierarchy::init();

  const Context& ctx = system();
  if (auto fact = ctx.doc.app.interfaces<Scenario::CSPCoherencyCheckerList>().get())
  {
    m_checker = fact->make(process(), ctx.doc.app, m_properties);
    if (m_checker)
    {
      m_properties.timenodes[Id<Scenario::TimeNodeModel>(0)].date = 0;
      m_checker->computeDisplacement(m_pastTn, m_properties);
    }
  }
}

void ScenarioComponent::cleanup()
{
  clear();
}

void ScenarioComponentBase::stop()
{
  m_executingConstraints.clear();
  ProcessComponent::stop();
}

std::function<void ()> ScenarioComponentBase::removing(
    const Scenario::ConstraintModel& e, ConstraintComponent& c)
{
  auto it = m_ossia_constraints.find(e.id());
  if(it != m_ossia_constraints.end())
  {
   std::shared_ptr<ossia::scenario> proc = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);
    m_ctx.executionQueue.enqueue([proc,cstr=c.OSSIAConstraint()] {
      if(cstr)
      {
        auto& next = cstr->getStartEvent().nextTimeConstraints();
        auto next_it = ossia::find(next, cstr);
        if(next_it != next.end())
          next.erase(next_it);

        auto& prev = cstr->getEndEvent().previousTimeConstraints();
        auto prev_it = ossia::find(prev, cstr);
        if(prev_it != prev.end())
          prev.erase(prev_it);

        proc->removeTimeConstraint(cstr);
      }
    });

    c.cleanup();

    return [=] { m_ossia_constraints.erase(it); };
  }
  return {};
}

std::function<void ()> ScenarioComponentBase::removing(
    const Scenario::TimeNodeModel& e, TimeNodeComponent& c)
{
  // FIXME this will certainly break stuff WRT member variables, coherency checker, etc.
  auto it = m_ossia_timenodes.find(e.id());
  if(it != m_ossia_timenodes.end())
  {
    std::shared_ptr<ossia::scenario> proc = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);
    m_ctx.executionQueue.enqueue([proc,tn=c.OSSIATimeNode()] {
      tn->cleanup();
      proc->removeTimeNode(tn);
    });

    it->second->cleanup();

    return [=] { m_ossia_timenodes.erase(it); };
  }
  return {};
}

std::function<void ()> ScenarioComponentBase::removing(
    const Scenario::EventModel& e, EventComponent& c)
{
  auto it = m_ossia_timeevents.find(e.id());
  if(it != m_ossia_timeevents.end())
  {
    m_ctx.executionQueue.enqueue([ev=c.OSSIAEvent()] {
      ev->cleanup();
      ev->getTimeNode().remove(ev);
    });

    c.cleanup();

    return [=] { m_ossia_timeevents.erase(it); };
  }
  return {};
}

std::function<void ()> ScenarioComponentBase::removing(
    const Scenario::StateModel& e, StateComponent& c)
{
  auto it = m_ossia_states.find(e.id());
  if(it != m_ossia_states.end())
  {
    c.onDelete();
    return [=] { m_ossia_states.erase(it); };
  }
  return {};
}

static void ScenarioConstraintCallback(
    ossia::time_value, ossia::time_value, const ossia::state& element)
{
}

template<>
ConstraintComponent* ScenarioComponentBase::make<ConstraintComponent, Scenario::ConstraintModel>(
    const Id<iscore::Component>& id,
    Scenario::ConstraintModel& cst)
{
  // Create the mapping object
  std::shared_ptr<ConstraintComponent> elt = std::make_shared<ConstraintComponent>(cst, m_ctx, id, this);
  m_ossia_constraints.insert({cst.id(), elt});

  // Find the elements related to this constraint.
  auto& te = m_ossia_timeevents;

  ISCORE_ASSERT(te.find(process().state(cst.startState()).eventId()) != te.end());
  ISCORE_ASSERT(te.find(process().state(cst.endState()).eventId()) != te.end());
  auto ossia_sev = te.at(process().state(cst.startState()).eventId());
  auto ossia_eev = te.at(process().state(cst.endState()).eventId());

  // Create the time_constraint
  auto dur = elt->makeDurations();
  auto ossia_cst = std::make_shared<ossia::time_constraint>(
        ScenarioConstraintCallback,
        *ossia_sev->OSSIAEvent(),
        *ossia_eev->OSSIAEvent(),
        dur.defaultDuration,
        dur.minDuration,
        dur.maxDuration);

  elt->onSetup(ossia_cst, dur, false);

  // The adding of the time_constraint has to be done in the edition thread.
  m_ctx.executionQueue.enqueue(
        [thisP=shared_from_this()
        ,ossia_sev,ossia_eev,ossia_cst
        ] {
    auto& sub = static_cast<ScenarioComponentBase&>(*thisP);

    if(auto sev = ossia_sev->OSSIAEvent())
      sev->nextTimeConstraints().push_back(ossia_cst);
    if(auto eev = ossia_eev->OSSIAEvent())
      eev->previousTimeConstraints().push_back(ossia_cst);

    sub.OSSIAProcess().addTimeConstraint(ossia_cst);
  });
  return elt.get();
}

template<>
StateComponent* ScenarioComponentBase::make<StateComponent, Scenario::StateModel>(
    const Id<iscore::Component>& id,
    Scenario::StateModel& iscore_state)
{
  auto elt = std::make_shared<StateComponent>(iscore_state, m_ctx, id, this);

  auto& events = m_ossia_timeevents;

  ISCORE_ASSERT(events.find(iscore_state.eventId()) != events.end());
  auto ossia_ev = events.at(iscore_state.eventId());

  m_ctx.executionQueue.enqueue([elt,ev=ossia_ev] {
    if(auto e = ev->OSSIAEvent())
      elt->onSetup(e);
  });

  m_ossia_states.insert({iscore_state.id(), elt});

  return elt.get();
}

template<>
EventComponent* ScenarioComponentBase::make<EventComponent, Scenario::EventModel>(
    const Id<iscore::Component>& id,
    Scenario::EventModel& ev)
{
  // Create the component
  auto elt = std::make_shared<EventComponent>(ev, m_ctx, id, this);
  m_ossia_timeevents.insert({ev.id(), elt});

  // Find the parent time node for the new event
  auto& nodes = m_ossia_timenodes;
  ISCORE_ASSERT(nodes.find(ev.timeNode()) != nodes.end());
  auto tn = nodes.at(ev.timeNode());

  // Create the event
  auto ossia_ev = std::make_shared<ossia::time_event>(
        [=](ossia::time_event::Status st) { return eventCallback(*elt, st); },
        *tn->OSSIATimeNode(),
        ossia::expression_ptr{});

  elt->onSetup(ossia_ev, elt->makeExpression(), (ossia::time_event::OffsetBehavior) (ev.offsetBehavior()));

  // The event is inserted in the API edition thread
  m_ctx.executionQueue.enqueue(
        [event=ossia_ev,time_node=tn->OSSIATimeNode()]
  {
    ISCORE_ASSERT(event);
    ISCORE_ASSERT(time_node);
    time_node->insert(time_node->timeEvents().begin(), event);
  });

  return elt.get();
}

template<>
TimeNodeComponent* ScenarioComponentBase::make<TimeNodeComponent, Scenario::TimeNodeModel>(
    const Id<iscore::Component>& id,
    Scenario::TimeNodeModel& tn)
{
  // Create the object
  auto elt = std::make_shared<TimeNodeComponent>(tn, m_ctx, id, this);
  m_ossia_timenodes.insert({tn.id(), elt});

  bool must_add = false;
  // The OSSIA API already creates the start time node so we must use it if available
  std::shared_ptr<ossia::time_node> ossia_tn;
  if (tn.id() ==  Scenario::startId<Scenario::TimeNodeModel>())
  {
    ossia_tn = OSSIAProcess().getStartTimeNode();
  }
  else
  {
    ossia_tn = std::make_shared<ossia::time_node>();
    must_add = true;
  }

  // Setup the object
  elt->onSetup(ossia_tn, elt->makeTrigger());

  // What happens when a time node's status change
  ossia_tn->triggered.add_callback([thisP=shared_from_this(),elt] {
    auto& sub = static_cast<ScenarioComponentBase&>(*thisP);
    return sub.timeNodeCallback(
          elt.get(), sub.m_parent_constraint.OSSIAConstraint()->getDate());
  });

  // Changing the running API structures
  m_ctx.executionQueue.enqueue(
        [
        thisP=shared_from_this()
        ,ossia_tn
        ,must_add]
  {
    auto& sub = static_cast<ScenarioComponentBase&>(*thisP);

    if(must_add)
      sub.OSSIAProcess().addTimeNode(ossia_tn);
  });

  return elt.get();
}

void ScenarioComponentBase::startConstraintExecution(
    const Id<Scenario::ConstraintModel>& id)
{
  auto& cst = process().constraints.at(id);
  if (m_executingConstraints.find(id) == m_executingConstraints.end())
    m_executingConstraints.insert(std::make_pair(cst.id(), &cst));

  auto it = m_ossia_constraints.find(id);
  if(it != m_ossia_constraints.end())
    it->second->executionStarted();
}

void ScenarioComponentBase::disableConstraintExecution(
    const Id<Scenario::ConstraintModel>& id)
{
  auto& cst = process().constraints.at(id);
  cst.setExecutionState(Scenario::ConstraintExecutionState::Disabled);
}

void ScenarioComponentBase::stopConstraintExecution(
    const Id<Scenario::ConstraintModel>& id)
{
  m_executingConstraints.erase(id);
  auto it = m_ossia_constraints.find(id);
  if(it != m_ossia_constraints.end())
    it->second->executionStopped();
}

void ScenarioComponentBase::eventCallback(
    EventComponent& ev, ossia::time_event::Status newStatus)
{
  auto the_event = const_cast<Scenario::EventModel*>(&ev.iscoreEvent());
  the_event->setStatus(static_cast<Scenario::ExecutionStatus>(newStatus));

  for (auto& state : the_event->states())
  {
    auto& iscore_state = process().states.at(state);

    if (auto& c = iscore_state.previousConstraint())
    {
      m_properties.constraints[*c].status
          = static_cast<Scenario::ExecutionStatus>(newStatus);
    }

    switch (newStatus)
    {
      case ossia::time_event::Status::NONE:
        break;
      case ossia::time_event::Status::PENDING:
        break;
      case ossia::time_event::Status::HAPPENED:
      {
        // Stop the previous constraints clocks,
        // start the next constraints clocks
        if (iscore_state.previousConstraint())
        {
          stopConstraintExecution(*iscore_state.previousConstraint());
        }

        if (iscore_state.nextConstraint())
        {
          startConstraintExecution(*iscore_state.nextConstraint());
        }
        break;
      }

      case ossia::time_event::Status::DISPOSED:
      {
        // TODO disable the constraints graphically
        if (iscore_state.nextConstraint())
        {
          disableConstraintExecution(*iscore_state.nextConstraint());
        }
        break;
      }
      default:
        ISCORE_TODO;
        break;
    }
  }
}

void ScenarioComponentBase::timeNodeCallback(
    TimeNodeComponent* tn, ossia::time_value date)
{
  if (m_checker)
  {
    m_pastTn.push_back(tn->iscoreTimeNode().id());

    // Fix Timenode
    auto& curTnProp = m_properties.timenodes[tn->iscoreTimeNode().id()];
    curTnProp.date = double(date);
    curTnProp.date_max = curTnProp.date;
    curTnProp.date_min = curTnProp.date;
    curTnProp.status = Scenario::ExecutionStatus::Happened;

    // Fix previous constraints
    auto previousCstrs
        = Scenario::previousConstraints(tn->iscoreTimeNode(), process());

    for (auto& cstrId : previousCstrs)
    {
      auto& startTn
          = Scenario::startTimeNode(process().constraint(cstrId), process());
      auto& cstrProp = m_properties.constraints[cstrId];

      cstrProp.newMin.setMSecs(
            curTnProp.date - m_properties.timenodes[startTn.id()].date);
      cstrProp.newMax = cstrProp.newMin;

      cstrProp.status = Scenario::ExecutionStatus::Happened;
    }

    // Compute new values
    m_checker->computeDisplacement(m_pastTn, m_properties);

    // Update constraints
    for (auto& cstr : m_ossia_constraints)
    {
      auto ossiaCstr = cstr.second->OSSIAConstraint();

      auto tmin = m_properties.constraints[cstr.first].newMin.msec();
      ossiaCstr->setDurationMin(ossia::time_value{tmin});

      auto tmax = m_properties.constraints[cstr.first].newMax.msec();
      ossiaCstr->setDurationMax(ossia::time_value{tmax});
    }
  }
}


}
}
