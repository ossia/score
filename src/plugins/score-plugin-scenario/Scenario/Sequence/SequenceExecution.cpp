// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioExecution.hpp>
#include <Scenario/Sequence/SequenceExecution.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_sync.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Execution::SequenceComponentBase)

namespace Execution
{

SequenceComponentBase::SequenceComponentBase(
    Sequence::SequenceModel& element, const Context& ctx, QObject* parent)
    : ProcessComponent_T<Sequence::SequenceModel, ossia::scenario>{
        element, ctx, "SequenceComponent", nullptr}
    , m_ctx{ctx}
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  this->setObjectName("OSSIASequenceElement");

  m_ossia_process = std::make_shared<ossia::scenario>();
  node = m_ossia_process->node;
  ((ossia::nodes::forward_node*)this->node.get())
      ->audio_in.sources.reserve(element.intervals.size() * 2);

  connect(
      this, &SequenceComponentBase::sig_eventCallback, this,
      &SequenceComponentBase::eventCallback, Qt::QueuedConnection);
}

SequenceComponentBase::~SequenceComponentBase()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
}

SequenceComponent::SequenceComponent(
    Sequence::SequenceModel& proc, const Context& ctx, QObject* parent)
    : SequenceComponentHierarchy{score::lazy_init_t{}, proc, ctx, parent}
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
}

void SequenceComponent::lazy_init()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  SequenceComponentHierarchy::init();
}

void SequenceComponent::cleanup()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  clear();
  ProcessComponent::cleanup();
}

void SequenceComponentBase::stop()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  for(const auto& itv : m_ossia_intervals)
    itv.second->stop();
  m_executingIntervals.clear();
  ProcessComponent::stop();
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::IntervalModel& e, IntervalComponent& c)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto it = m_ossia_intervals.find(e.id());
  if(it != m_ossia_intervals.end())
  {
    std::shared_ptr<ossia::scenario> proc
        = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);

    std::shared_ptr<ossia::time_event> start_ev, end_ev;
    auto start_ev_it
        = m_ossia_timeevents.find(Scenario::startEvent(e, this->process()).id());
    if(start_ev_it != m_ossia_timeevents.end())
      start_ev = start_ev_it->second->OSSIAEvent();
    auto end_ev_it = m_ossia_timeevents.find(Scenario::endEvent(e, this->process()).id());
    if(end_ev_it != m_ossia_timeevents.end())
      end_ev = end_ev_it->second->OSSIAEvent();

    m_ctx.executionQueue.enqueue([proc, cstr = c.OSSIAInterval(), start_ev, end_ev] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
      if(cstr)
      {
        if(start_ev)
        {
          auto& next = start_ev->next_time_intervals();
          auto next_it = ossia::find(next, cstr);
          if(next_it != next.end())
            next.erase(next_it);
        }

        if(end_ev)
        {
          auto& prev = end_ev->previous_time_intervals();
          auto prev_it = ossia::find(prev, cstr);
          if(prev_it != prev.end())
            prev.erase(prev_it);
        }

        proc->remove_time_interval(cstr);
      }
    });

    c.cleanup(it->second);

    return [this, id = e.id()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      m_ossia_intervals.erase(id);
    };
  }
  return {};
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::TimeSyncModel& e, TimeSyncComponent& c)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto it = m_ossia_timesyncs.find(e.id());
  if(it != m_ossia_timesyncs.end())
  {
    if(e.id() != Scenario::startId<Scenario::TimeSyncModel>())
    {
      std::shared_ptr<ossia::scenario> proc
          = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);
      in_exec([proc, tn = c.OSSIATimeSync()] {
        OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
        proc->remove_time_sync(tn);
      });
    }
    it->second->cleanup(it->second);

    return [this, id = e.id()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      m_ossia_timesyncs.erase(id);
    };
  }
  return {};
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::EventModel& e, EventComponent& c)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto ev_it = m_ossia_timeevents.find(e.id());
  if(ev_it != m_ossia_timeevents.end())
  {
    auto tn_it = m_ossia_timesyncs.find(e.timeSync());
    if(tn_it != m_ossia_timesyncs.end() && tn_it->second)
    {
      if(auto tn = tn_it->second->OSSIATimeSync())
      {
        auto weak_tn = std::weak_ptr{tn};
        auto weak_ev = std::weak_ptr{c.OSSIAEvent()};

        m_ctx.executionQueue.enqueue([weak_tn, weak_ev] {
          OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
          if(auto e = weak_ev.lock())
          {
            if(auto t = weak_tn.lock())
              t->remove(e);
            e->cleanup();
          }
        });

        c.cleanup(ev_it->second);
        return [this, id = e.id()] { m_ossia_timeevents.erase(id); };
      }
    }

    m_ctx.executionQueue.enqueue([ev = c.OSSIAEvent()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
      ev->cleanup();
    });
    c.cleanup(ev_it->second);
    return [this, id = e.id()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      m_ossia_timeevents.erase(id);
    };
  }
  return {};
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::StateModel& e, StateComponent& c)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto it = m_ossia_states.find(e.id());
  if(it != m_ossia_states.end())
  {
    c.onDelete();
    return [this, id = e.id()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      m_ossia_states.erase(id);
    };
  }
  return {};
}

static void SequenceIntervalCallback(bool, ossia::time_value) { }

template <>
IntervalComponent*
SequenceComponentBase::make<IntervalComponent, Scenario::IntervalModel>(
    Scenario::IntervalModel& cst)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  std::shared_ptr<ossia::scenario> proc
      = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);

  std::shared_ptr<IntervalComponent> elt
      = std::make_shared<IntervalComponent>(cst, proc, m_ctx, this);
  m_ossia_intervals.insert({cst.id(), elt});

  auto& te = m_ossia_timeevents;

  SCORE_ASSERT(te.find(process().state(cst.startState()).eventId()) != te.end());
  SCORE_ASSERT(te.find(process().state(cst.endState()).eventId()) != te.end());
  auto ossia_sev = te.at(process().state(cst.startState()).eventId());
  auto ossia_eev = te.at(process().state(cst.endState()).eventId());

  auto dur = elt->makeDurations();
  auto ossia_cst = std::make_shared<ossia::time_interval>(
      smallfun::function<void(bool, ossia::time_value), 32>{&SequenceIntervalCallback},
      *ossia_sev->OSSIAEvent(), *ossia_eev->OSSIAEvent(), dur.defaultDuration,
      dur.minDuration, dur.maxDuration);

  ossia_cst->node->prepare(*m_ctx.execState);
  elt->onSetup(elt, ossia_cst, dur);

  const bool prop = cst.graphal() ? false : cst.outlet->propagate();
  in_exec([g = system().execGraph, proc, ossia_sev, ossia_eev, ossia_cst, prop] {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    if(auto sev = ossia_sev->OSSIAEvent())
      sev->next_time_intervals().push_back(ossia_cst);
    if(auto eev = ossia_eev->OSSIAEvent())
      eev->previous_time_intervals().push_back(ossia_cst);

    proc->add_time_interval(ossia_cst);

    if(prop)
    {
      auto cable = g->allocate_edge(
          ossia::immediate_glutton_connection{}, ossia_cst->node->root_outputs()[0],
          proc->node->root_inputs()[0], ossia_cst->node, proc->node);
      g->connect(cable);
    }
  });
  return elt.get();
}

template <>
StateComponent* SequenceComponentBase::make<StateComponent, Scenario::StateModel>(
    Scenario::StateModel& st)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto& events = m_ossia_timeevents;
  SCORE_ASSERT(events.find(st.eventId()) != events.end());
  auto ossia_ev = events.at(st.eventId());

  auto elt = std::make_shared<StateComponent>(st, ossia_ev->OSSIAEvent(), m_ctx, this);
  m_ossia_states.insert({st.id(), elt});
  return elt.get();
}

template <>
EventComponent* SequenceComponentBase::make<EventComponent, Scenario::EventModel>(
    Scenario::EventModel& ev)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto elt = std::make_shared<EventComponent>(ev, m_ctx, this);
  m_ossia_timeevents.insert({ev.id(), elt});

  auto& nodes = m_ossia_timesyncs;
  SCORE_ASSERT(nodes.find(ev.timeSync()) != nodes.end());
  auto tn = nodes.at(ev.timeSync());

  std::weak_ptr<EventComponent> weak_ev = elt;
  std::weak_ptr<SequenceComponentBase> thisP
      = std::dynamic_pointer_cast<SequenceComponentBase>(shared_from_this());
  auto ev_cb = [qed_ptr = weak_edit, weak_ev, thisP](ossia::time_event::status st) {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    if(auto elt = weak_ev.lock())
    {
      if(auto sc = thisP.lock())
      {
        if(auto q = qed_ptr.lock())
          q->enqueue([sc, elt, st] {
            OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
            sc->sig_eventCallback(sc, elt, st);
          });
      }
    }
  };
  auto ossia_ev = std::make_shared<ossia::time_event>(
      std::move(ev_cb), *tn->OSSIATimeSync(), ossia::expression_ptr{});

  elt->onSetup(
      ossia_ev, elt->makeExpression(),
      (ossia::time_event::offset_behavior)(ev.offsetBehavior()));

  m_ctx.executionQueue.enqueue([event = ossia_ev, time_sync = tn->OSSIATimeSync()] {
    SCORE_ASSERT(event);
    SCORE_ASSERT(time_sync);
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    time_sync->insert(time_sync->get_time_events().begin(), event);
  });

  return elt.get();
}

template <>
TimeSyncComponent*
SequenceComponentBase::make<TimeSyncComponent, Scenario::TimeSyncModel>(
    Scenario::TimeSyncModel& tn)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto elt = std::make_shared<TimeSyncComponent>(tn, m_ctx, this);
  m_ossia_timesyncs.insert({tn.id(), elt});

  bool must_add = false;
  std::shared_ptr<ossia::time_sync> ossia_tn;
  if(tn.id() == Scenario::startId<Scenario::TimeSyncModel>())
  {
    ossia_tn = OSSIAProcess().get_start_time_sync();
  }
  else
  {
    ossia_tn = std::make_shared<ossia::time_sync>();
    must_add = true;
  }

  elt->onSetup(ossia_tn, elt->makeTrigger());

  if(must_add)
  {
    auto ossia_sc = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);
    m_ctx.executionQueue.enqueue([ossia_sc, ossia_tn] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
      ossia_sc->add_time_sync(ossia_tn);
    });
  }

  return elt.get();
}

void SequenceComponentBase::startIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto& cst = process().intervals.at(id);
  if(m_executingIntervals.find(id) == m_executingIntervals.end())
    m_executingIntervals.insert(std::make_pair(cst.id(), &cst));

  auto it = m_ossia_intervals.find(id);
  if(it != m_ossia_intervals.end())
    it->second->executionStarted();

  cst.setExecutionState(Scenario::IntervalExecutionState::Enabled);
}

void SequenceComponentBase::disableIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto& cst = process().intervals.at(id);
  cst.setExecutionState(Scenario::IntervalExecutionState::Disabled);
}

void SequenceComponentBase::stopIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  m_executingIntervals.erase(id);
  auto it = m_ossia_intervals.find(id);
  if(it != m_ossia_intervals.end())
    it->second->executionStopped();
}

void SequenceComponentBase::eventCallback(
    std::weak_ptr<SequenceComponentBase> self, std::shared_ptr<EventComponent> ev,
    ossia::time_event::status newStatus)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  auto p = self.lock();
  if(!p)
    return;
  auto the_event = const_cast<Scenario::EventModel*>(ev->scoreEvent());
  if(!the_event)
    return;

  the_event->setStatus(static_cast<Scenario::ExecutionStatus>(newStatus), process());

  for(auto& state : the_event->states())
  {
    auto& score_state = process().states.at(state);

    if(auto& c = score_state.previousInterval())
    {
      // update properties
    }

    switch(newStatus)
    {
      case ossia::time_event::status::NONE:
        break;
      case ossia::time_event::status::PENDING:
        break;
      case ossia::time_event::status::HAPPENED: {
        if(score_state.previousInterval())
          stopIntervalExecution(*score_state.previousInterval());
        if(score_state.nextInterval())
          startIntervalExecution(*score_state.nextInterval());
        break;
      }
      case ossia::time_event::status::DISPOSED: {
        if(score_state.nextInterval())
          disableIntervalExecution(*score_state.nextInterval());
        break;
      }
      default:
        SCORE_TODO;
        break;
    }
  }
}

}
