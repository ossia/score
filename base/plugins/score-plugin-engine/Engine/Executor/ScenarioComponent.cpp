// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/state/state_element.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_sync.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Engine/score2OSSIA.hpp>
#include <QDebug>
#include <algorithm>
#include <core/document/Document.hpp>
#include <score/document/DocumentInterface.hpp>
#include <vector>

#include "ScenarioComponent.hpp"
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/EventComponent.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/Executor/StateComponent.hpp>
#include <Engine/Executor/TimeSyncComponent.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Scenario/ExecutionChecker/CSPCoherencyCheckerInterface.hpp>
#include <Scenario/ExecutionChecker/CSPCoherencyCheckerList.hpp>
#include <Scenario/ExecutionChecker/CoherencyCheckerFactoryInterface.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <ossia/editor/loop/loop.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <ossia/dataflow/graph/graph_utils.hpp>

namespace Engine
{
namespace Execution
{
ScenarioComponentBase::ScenarioComponentBase(
    Scenario::ProcessModel& element,
    const Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ProcessComponent_T<Scenario::ProcessModel, ossia::scenario>{
                                                                element, ctx,
                                                                id,
                                                                "ScenarioComponent",
                                                                nullptr}
  , m_ctx{ctx}
{
  this->setObjectName("OSSIAScenarioElement");

  // Setup of the OSSIA API Part
  m_ossia_process = std::make_shared<ossia::scenario>();
  node = m_ossia_process->node;
  connect(this, &ScenarioComponentBase::sig_eventCallback,
          this, &ScenarioComponentBase::eventCallback, Qt::QueuedConnection);

  // Note : the hierarchical scenario shall create the time syncs first.
  // A better way would be :
  // * Either to not have a dependency ordering, which would require two passes
  // * Or to have the HierarchicalScenario take a variadic amount of stuff and init them in the right order.
}

ScenarioComponentBase::~ScenarioComponentBase()
{

}

ScenarioComponent::ScenarioComponent(
    Scenario::ProcessModel& proc,
    const Context& ctx,
    const Id<score::Component>& id,
    QObject* parent):
  ScenarioComponentHierarchy{score::lazy_init_t{}, proc, ctx, id, parent}
{
}

void ScenarioComponent::init()
{
  // set-up the ports
  const Context& ctx = system();
  auto ossia_sc = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);

  ScenarioComponentHierarchy::init();


  if (auto fact = ctx.doc.app.interfaces<Scenario::CSPCoherencyCheckerList>().get())
  {
    m_checker = fact->make(process(), ctx.doc.app, m_properties);
    if (m_checker)
    {
      m_properties.timesyncs[Id<Scenario::TimeSyncModel>(0)].date = 0;
      m_checker->computeDisplacement(m_pastTn, m_properties);
    }
  }
}

void ScenarioComponent::cleanup()
{
  clear();
  ProcessComponent::cleanup();
}

void ScenarioComponentBase::stop()
{
  for(const auto& itv : m_ossia_intervals)
  {
    itv.second->stop();
  }
  m_executingIntervals.clear();
  ProcessComponent::stop();

  for(Scenario::EventModel& e : process().events)
  {
    e.setStatus(Scenario::ExecutionStatus::Editing, process());
  }
}

std::function<void ()> ScenarioComponentBase::removing(
    const Scenario::IntervalModel& e, IntervalComponent& c)
{
  auto it = m_ossia_intervals.find(e.id());
  if(it != m_ossia_intervals.end())
  {
   std::shared_ptr<ossia::scenario> proc = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);
    m_ctx.executionQueue.enqueue([proc,cstr=c.OSSIAInterval()] {
      if(cstr)
      {
        auto& next = cstr->get_start_event().next_time_intervals();
        auto next_it = ossia::find(next, cstr);
        if(next_it != next.end())
          next.erase(next_it);

        auto& prev = cstr->get_end_event().previous_time_intervals();
        auto prev_it = ossia::find(prev, cstr);
        if(prev_it != prev.end())
          prev.erase(prev_it);

        proc->remove_time_interval(cstr);
      }
    });

    c.cleanup(it->second);

    return [=] { m_ossia_intervals.erase(it); };
  }
  return {};
}

std::function<void ()> ScenarioComponentBase::removing(
    const Scenario::TimeSyncModel& e, TimeSyncComponent& c)
{
  // FIXME this will certainly break stuff WRT member variables, coherency checker, etc.
  auto it = m_ossia_timesyncs.find(e.id());
  if(it != m_ossia_timesyncs.end())
  {
    if(e.id() != Scenario::startId<Scenario::TimeSyncModel>())
    {
      std::shared_ptr<ossia::scenario> proc = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);
      m_ctx.executionQueue.enqueue([proc,tn=c.OSSIATimeSync()] {
        proc->remove_time_sync(tn);
      });
    }
    it->second->cleanup();

    return [=] { m_ossia_timesyncs.erase(it); };
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
      ev->get_time_sync().remove(ev);
      ev->cleanup();
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

static void ScenarioIntervalCallback(
    double, ossia::time_value)
{
}

template<>
IntervalComponent* ScenarioComponentBase::make<IntervalComponent, Scenario::IntervalModel>(
    const Id<score::Component>& id,
    Scenario::IntervalModel& cst)
{
  // Create the mapping object
  std::shared_ptr<IntervalComponent> elt = std::make_shared<IntervalComponent>(cst, m_ctx, id, this);
  m_ossia_intervals.insert({cst.id(), elt});

  // Find the elements related to this interval.
  auto& te = m_ossia_timeevents;

  SCORE_ASSERT(te.find(process().state(cst.startState()).eventId()) != te.end());
  SCORE_ASSERT(te.find(process().state(cst.endState()).eventId()) != te.end());
  auto ossia_sev = te.at(process().state(cst.startState()).eventId());
  auto ossia_eev = te.at(process().state(cst.endState()).eventId());

  // Create the time_interval
  auto dur = elt->makeDurations();
  auto ossia_cst = std::make_shared<ossia::time_interval>(
        smallfun::function<void(double, ossia::time_value), 32>{&ScenarioIntervalCallback},
        *ossia_sev->OSSIAEvent(),
        *ossia_eev->OSSIAEvent(),
        dur.defaultDuration,
        dur.minDuration,
        dur.maxDuration);

  elt->onSetup(elt, ossia_cst, dur, false);


  // The adding of the time_interval has to be done in the edition thread.
  std::shared_ptr<ossia::scenario> proc = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);

  m_ctx.executionQueue.enqueue(
        [g=system().plugin.execGraph
        ,proc
        ,ossia_sev,ossia_eev,ossia_cst
        ] {
    if(auto sev = ossia_sev->OSSIAEvent())
      sev->next_time_intervals().push_back(ossia_cst);
    if(auto eev = ossia_eev->OSSIAEvent())
      eev->previous_time_intervals().push_back(ossia_cst);

    proc->add_time_interval(ossia_cst);

    auto cable = ossia::make_edge(
                   ossia::immediate_glutton_connection{}
                   , ossia_cst->node->outputs()[0]
                   , proc->node->inputs()[0]
                   , ossia_cst->node
                   , proc->node);
    g->connect(cable);
  });
  return elt.get();
}

template<>
StateComponent* ScenarioComponentBase::make<StateComponent, Scenario::StateModel>(
    const Id<score::Component>& id,
    Scenario::StateModel& st)
{
  auto elt = std::make_shared<StateComponent>(st, m_ctx, id, this);

  auto& events = m_ossia_timeevents;

  SCORE_ASSERT(events.find(st.eventId()) != events.end());
  auto ossia_ev = events.at(st.eventId());

  m_ctx.executionQueue.enqueue([elt,ev=ossia_ev] {
    if(auto e = ev->OSSIAEvent())
      elt->onSetup(e);
  });

  m_ossia_states.insert({st.id(), elt});

  return elt.get();
}

template<>
EventComponent* ScenarioComponentBase::make<EventComponent, Scenario::EventModel>(
    const Id<score::Component>& id,
    Scenario::EventModel& ev)
{
  // Create the component
  auto elt = std::make_shared<EventComponent>(ev, m_ctx, id, this);
  m_ossia_timeevents.insert({ev.id(), elt});

  // Find the parent time sync for the new event
  auto& nodes = m_ossia_timesyncs;
  SCORE_ASSERT(nodes.find(ev.timeSync()) != nodes.end());
  auto tn = nodes.at(ev.timeSync());

  // Create the event
  std::weak_ptr<EventComponent> weak_ev = elt;

  std::weak_ptr<ScenarioComponentBase> thisP = std::dynamic_pointer_cast<ScenarioComponentBase>(shared_from_this());
  auto ev_cb = [weak_ev,thisP](ossia::time_event::status st) {
    if(auto elt = weak_ev.lock())
    {
      if(auto sc = thisP.lock())
      {
        sc->sig_eventCallback(elt, st);
      }
    }
  };
  auto ossia_ev = std::make_shared<ossia::time_event>(
                    std::move(ev_cb),
                    *tn->OSSIATimeSync(),
                    ossia::expression_ptr{});

  elt->onSetup(ossia_ev, elt->makeExpression(), (ossia::time_event::offset_behavior) (ev.offsetBehavior()));

  // The event is inserted in the API edition thread
  m_ctx.executionQueue.enqueue(
        [event=ossia_ev,time_sync=tn->OSSIATimeSync()]
  {
    SCORE_ASSERT(event);
    SCORE_ASSERT(time_sync);
    time_sync->insert(time_sync->get_time_events().begin(), event);
  });

  return elt.get();
}

template<>
TimeSyncComponent* ScenarioComponentBase::make<TimeSyncComponent, Scenario::TimeSyncModel>(
    const Id<score::Component>& id,
    Scenario::TimeSyncModel& tn)
{
  // Create the object
  auto elt = std::make_shared<TimeSyncComponent>(tn, m_ctx, id, this);
  m_ossia_timesyncs.insert({tn.id(), elt});

  bool must_add = false;
  // The OSSIA API already creates the start time sync so we must use it if available
  std::shared_ptr<ossia::time_sync> ossia_tn;
  if (tn.id() ==  Scenario::startId<Scenario::TimeSyncModel>())
  {
    ossia_tn = OSSIAProcess().get_start_time_sync();
    qDebug() << "root" << tn.expression().toString();
  }
  else
  {
    ossia_tn = std::make_shared<ossia::time_sync>();
    qDebug() << "non root" << tn.expression().toString();
    must_add = true;
  }

  // Setup the object
  elt->onSetup(ossia_tn, elt->makeTrigger());

  // What happens when a time sync's status change
  ossia_tn->triggered.add_callback([thisP=shared_from_this(),elt] {
    auto& sub = static_cast<ScenarioComponentBase&>(*thisP);
    return sub.timeSyncCallback(
          elt.get(), ossia::time_value{}/* TODO WTF sub.m_parent_interval.OSSIAInterval()->get_date()*/);
  });

  // Changing the running API structures
  if(must_add)
  {
    if(auto ossia_sc = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process))
    {
      m_ctx.executionQueue.enqueue(
            [ossia_sc,ossia_tn] {
        ossia_sc->add_time_sync(ossia_tn);
      });
    }
  }

  return elt.get();
}

void ScenarioComponentBase::startIntervalExecution(
    const Id<Scenario::IntervalModel>& id)
{
  auto& cst = process().intervals.at(id);
  if (m_executingIntervals.find(id) == m_executingIntervals.end())
    m_executingIntervals.insert(std::make_pair(cst.id(), &cst));

  auto it = m_ossia_intervals.find(id);
  if(it != m_ossia_intervals.end())
    it->second->executionStarted();
}

void ScenarioComponentBase::disableIntervalExecution(
    const Id<Scenario::IntervalModel>& id)
{
  auto& cst = process().intervals.at(id);
  cst.setExecutionState(Scenario::IntervalExecutionState::Disabled);
}

void ScenarioComponentBase::stopIntervalExecution(
    const Id<Scenario::IntervalModel>& id)
{
  m_executingIntervals.erase(id);
  auto it = m_ossia_intervals.find(id);
  if(it != m_ossia_intervals.end())
    it->second->executionStopped();
}

void ScenarioComponentBase::eventCallback(
    std::shared_ptr<EventComponent> ev, ossia::time_event::status newStatus)
{
  auto the_event = const_cast<Scenario::EventModel*>(&ev->scoreEvent());
  the_event->setStatus(static_cast<Scenario::ExecutionStatus>(newStatus), process());

  for (auto& state : the_event->states())
  {
    auto& score_state = process().states.at(state);

    if (auto& c = score_state.previousInterval())
    {
      m_properties.intervals[*c].status
          = static_cast<Scenario::ExecutionStatus>(newStatus);
    }

    switch (newStatus)
    {
      case ossia::time_event::status::NONE:
        break;
      case ossia::time_event::status::PENDING:
        break;
      case ossia::time_event::status::HAPPENED:
      {
        // Stop the previous intervals clocks,
        // start the next intervals clocks
        if (score_state.previousInterval())
        {
          stopIntervalExecution(*score_state.previousInterval());
        }

        if (score_state.nextInterval())
        {
          startIntervalExecution(*score_state.nextInterval());
        }
        break;
      }

      case ossia::time_event::status::DISPOSED:
      {
        // TODO disable the intervals graphically
        if (score_state.nextInterval())
        {
          disableIntervalExecution(*score_state.nextInterval());
        }
        break;
      }
      default:
        SCORE_TODO;
        break;
    }
  }
}

void ScenarioComponentBase::timeSyncCallback(
    TimeSyncComponent* tn, ossia::time_value date)
{
  if (m_checker)
  {
    m_pastTn.push_back(tn->scoreTimeSync().id());

    // Fix Timenode
    auto& curTnProp = m_properties.timesyncs[tn->scoreTimeSync().id()];
    curTnProp.date = double(date);
    curTnProp.date_max = curTnProp.date;
    curTnProp.date_min = curTnProp.date;
    curTnProp.status = Scenario::ExecutionStatus::Happened;

    // Fix previous intervals
    auto previousCstrs
        = Scenario::previousIntervals(tn->scoreTimeSync(), process());

    for (auto& cstrId : previousCstrs)
    {
      auto& startTn
          = Scenario::startTimeSync(process().interval(cstrId), process());
      auto& cstrProp = m_properties.intervals[cstrId];

      cstrProp.newMin.setMSecs(
            curTnProp.date - m_properties.timesyncs[startTn.id()].date);
      cstrProp.newMax = cstrProp.newMin;

      cstrProp.status = Scenario::ExecutionStatus::Happened;
    }

    // Compute new values
    m_checker->computeDisplacement(m_pastTn, m_properties);

    // Update intervals
    for (auto& cstr : m_ossia_intervals)
    {
      auto ossiaCstr = cstr.second->OSSIAInterval();

      auto tmin = m_properties.intervals[cstr.first].newMin.msec();
      ossiaCstr->set_min_duration(ossia::time_value{tmin});

      auto tmax = m_properties.intervals[cstr.first].newMax.msec();
      ossiaCstr->set_max_duration(ossia::time_value{tmax});
    }
  }
}


}
}
