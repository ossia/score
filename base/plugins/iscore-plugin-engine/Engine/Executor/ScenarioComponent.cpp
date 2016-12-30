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
  m_ossia_process = new ossia::scenario;

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
  ScenarioComponentHierarchy{cst, proc, ctx, id, parent}
{
  if (auto fact
      = ctx.doc.app.interfaces<Scenario::CSPCoherencyCheckerList>()
            .get())
  {
    m_checker = fact->make(proc, ctx.doc.app, m_properties);
    if (m_checker)
    {
      m_properties.timenodes[Id<Scenario::TimeNodeModel>(0)].date = 0;
      m_checker->computeDisplacement(m_pastTn, m_properties);
    }
  }
}

void ScenarioComponentBase::stop()
{
  m_executingConstraints.clear();
  ProcessComponent::stop();
}

void ScenarioComponentBase::removeConstraint(const Id<Scenario::ConstraintModel>& id)
{
  auto it = m_ossia_constraints.find(id);
  if(it != m_ossia_constraints.end())
  {
    m_ctx.executionQueue.enqueue([&proc=OSSIAProcess(),cstr=it.value()->OSSIAConstraint()] {
      proc.removeTimeConstraint(cstr);
    });
    m_ossia_constraints.erase(it);
  }
}

template<>
void ScenarioComponentBase::removing(
    const Scenario::ConstraintModel& e, const ConstraintComponent& c)
{
  auto it = m_ossia_constraints.find(e.id());
  if(it != m_ossia_constraints.end())
  {
    m_ctx.executionQueue.enqueue([&proc=OSSIAProcess(),cstr=c.OSSIAConstraint()] {
      proc.removeTimeConstraint(cstr);
    });
    m_ossia_constraints.erase(it);
  }
}

template<>
void ScenarioComponentBase::removing(
    const Scenario::TimeNodeModel& e, const TimeNodeComponent& c)
{
  // FIXME this will certainly break stuff WRT member variables, coherency checker, etc.
  auto it = m_ossia_timenodes.find(e.id());
  if(it != m_ossia_timenodes.end())
  {
    m_ossia_timenodes.erase(it);
  }
}
template<>
void ScenarioComponentBase::removing(
    const Scenario::EventModel& e, const EventComponent& c)
{
  auto it = m_ossia_timeevents.find(e.id());
  if(it != m_ossia_timeevents.end())
  {
    m_ossia_timeevents.erase(it);
  }
}
template<>
void ScenarioComponentBase::removing(
    const Scenario::StateModel& e, const StateComponent& c)
{
  auto it = m_ossia_states.find(e.id());
  if(it != m_ossia_states.end())
  {
    c.onDelete();
    m_ossia_states.erase(it);
  }
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
  ISCORE_ASSERT(
      m_ossia_timeevents.find(process().state(cst.startState()).eventId())
      != m_ossia_timeevents.end());
  auto& ossia_sev
      = m_ossia_timeevents.at(process().state(cst.startState()).eventId());
  ISCORE_ASSERT(
      m_ossia_timeevents.find(process().state(cst.endState()).eventId())
      != m_ossia_timeevents.end());
  auto& ossia_eev
      = m_ossia_timeevents.at(process().state(cst.endState()).eventId());

  auto ossia_cst = ossia::time_constraint::create(
      ScenarioConstraintCallback,
      *ossia_sev->OSSIAEvent(),
      *ossia_eev->OSSIAEvent(),
      Engine::iscore_to_ossia::time(cst.duration.defaultDuration()),
      Engine::iscore_to_ossia::time(cst.duration.minDuration()),
      Engine::iscore_to_ossia::time(cst.duration.maxDuration()));

  m_ctx.executionQueue.enqueue([&proc=OSSIAProcess(),cstr=ossia_cst] {
    proc.addTimeConstraint(cstr);
  });

  // Create the mapping object
  auto elt = QSharedPointer<ConstraintComponent>::create(ossia_cst, cst, m_ctx, id, this);
  m_ossia_constraints.insert({cst.id(), elt});

  return elt.data();
}

template<>
StateComponent* ScenarioComponentBase::make<StateComponent, Scenario::StateModel>(
    const Id<iscore::Component>& id,
    Scenario::StateModel& iscore_state)
{
  auto elt = QSharedPointer<StateComponent>::create(iscore_state, m_ctx, id, this);

  m_ctx.executionQueue.enqueue([thisP=sharedFromThis(),elt,ev_id=iscore_state.eventId()] {
    auto sub = thisP.dynamicCast<ScenarioComponentBase>();
      auto& events = sub->m_ossia_timeevents;

      ISCORE_ASSERT(events.find(ev_id) != events.end());
      auto ossia_ev = events.at(ev_id);

      elt->onSetup(ossia_ev->OSSIAEvent());
    });

  m_ossia_states.insert({iscore_state.id(), elt});

  return elt.data();
}

template<>
EventComponent* ScenarioComponentBase::make<EventComponent, Scenario::EventModel>(
    const Id<iscore::Component>& id,
    Scenario::EventModel& const_ev)
{
  // TODO have a EventPlayAspect too
  auto& ev = const_cast<Scenario::EventModel&>(const_ev);
  ISCORE_ASSERT(
      m_ossia_timenodes.find(ev.timeNode()) != m_ossia_timenodes.end());
  auto ossia_tn = m_ossia_timenodes.at(ev.timeNode());

  std::shared_ptr<ossia::time_event> ossia_ev
      = *ossia_tn->OSSIATimeNode()->emplace(
          ossia_tn->OSSIATimeNode()->timeEvents().begin(),
          ossia::time_event::ExecutionCallback{},
          ossia::expressions::make_expression_true());

  // Create the mapping object
  auto elt = QSharedPointer<EventComponent>::create(ossia_ev, ev, m_ctx, id, this);
  m_ossia_timeevents.insert({ev.id(), elt});

  elt->OSSIAEvent()->setCallback(
      [=](ossia::time_event::Status st) { return eventCallback(*elt, st); });

  return elt.data();
}

template<>
TimeNodeComponent* ScenarioComponentBase::make<TimeNodeComponent, Scenario::TimeNodeModel>(
    const Id<iscore::Component>& id,
    Scenario::TimeNodeModel& tn)
{
  // Create the mapping object
  auto elt = QSharedPointer<TimeNodeComponent>::create(tn, m_ctx, id, this);
  m_ossia_timenodes.insert({tn.id(), elt});

  bool isStart = tn.id() ==  Scenario::startId<Scenario::TimeNodeModel>();
  m_ctx.executionQueue.enqueue([thisP=sharedFromThis(),elt,isStart] {
    auto sub = thisP.dynamicCast<ScenarioComponentBase>();
      std::shared_ptr<ossia::time_node> ossia_tn;
      if (isStart)
      {
        ossia_tn = sub->OSSIAProcess().getStartTimeNode();
      }
      else
      {
        ossia_tn = std::make_shared<ossia::time_node>();
        sub->OSSIAProcess().addTimeNode(ossia_tn);
      }

      elt->onSetup(ossia_tn);
      elt->OSSIATimeNode()->setCallback([=]() {
        return sub->timeNodeCallback(
            elt.data(), sub->m_parent_constraint.OSSIAConstraint()->getDate());
      });

  });

  return elt.data();
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
