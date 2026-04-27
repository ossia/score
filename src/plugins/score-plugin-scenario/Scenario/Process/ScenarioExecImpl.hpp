#pragma once
// Shared executor implementations for ScenarioComponentBase and
// SequenceComponentBase. Include only from their respective .cpp files.
// Both classes must declare: friend struct Execution::ScenarioExecImpl;

#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_sync.hpp>

namespace Execution
{

struct ScenarioExecImpl
{
  template <typename Base>
  static void stop_body(Base& self)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    for(const auto& itv : self.m_ossia_intervals)
      itv.second->stop();
    self.m_executingIntervals.clear();
  }

  template <typename Base>
  static std::function<void()>
  removing_interval(Base& self, const Scenario::IntervalModel& e, IntervalComponent& c)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    auto it = self.m_ossia_intervals.find(e.id());
    if(it == self.m_ossia_intervals.end())
      return {};

    std::shared_ptr<ossia::scenario> proc
        = std::dynamic_pointer_cast<ossia::scenario>(self.m_ossia_process);

    std::shared_ptr<ossia::time_event> start_ev, end_ev;
    auto start_ev_it
        = self.m_ossia_timeevents.find(Scenario::startEvent(e, self.process()).id());
    if(start_ev_it != self.m_ossia_timeevents.end())
      start_ev = start_ev_it->second->OSSIAEvent();
    auto end_ev_it
        = self.m_ossia_timeevents.find(Scenario::endEvent(e, self.process()).id());
    if(end_ev_it != self.m_ossia_timeevents.end())
      end_ev = end_ev_it->second->OSSIAEvent();

    self.m_ctx.executionQueue.enqueue(
        [proc, cstr = c.OSSIAInterval(), start_ev, end_ev] {
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
    return [&self, id = e.id()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      self.m_ossia_intervals.erase(id);
    };
  }

  template <typename Base>
  static std::function<void()>
  removing_timesync(Base& self, const Scenario::TimeSyncModel& e, TimeSyncComponent& c)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    auto it = self.m_ossia_timesyncs.find(e.id());
    if(it == self.m_ossia_timesyncs.end())
      return {};

    if(e.id() != Scenario::startId<Scenario::TimeSyncModel>())
    {
      std::shared_ptr<ossia::scenario> proc
          = std::dynamic_pointer_cast<ossia::scenario>(self.m_ossia_process);
      self.system().executionQueue.enqueue([proc, tn = c.OSSIATimeSync()] {
        OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
        proc->remove_time_sync(tn);
      });
    }
    it->second->cleanup(it->second);
    return [&self, id = e.id()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      self.m_ossia_timesyncs.erase(id);
    };
  }

  template <typename Base>
  static std::function<void()>
  removing_event(Base& self, const Scenario::EventModel& e, EventComponent& c)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    auto ev_it = self.m_ossia_timeevents.find(e.id());
    if(ev_it == self.m_ossia_timeevents.end())
      return {};

    auto tn_it = self.m_ossia_timesyncs.find(e.timeSync());
    if(tn_it != self.m_ossia_timesyncs.end() && tn_it->second)
    {
      if(auto tn = tn_it->second->OSSIATimeSync())
      {
        auto weak_tn = std::weak_ptr{tn};
        auto weak_ev = std::weak_ptr{c.OSSIAEvent()};
        self.m_ctx.executionQueue.enqueue([weak_tn, weak_ev] {
          OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
          if(auto e = weak_ev.lock())
          {
            if(auto t = weak_tn.lock())
              t->remove(e);
            e->cleanup();
          }
        });
        c.cleanup(ev_it->second);
        return [&self, id = e.id()] { self.m_ossia_timeevents.erase(id); };
      }
    }

    self.m_ctx.executionQueue.enqueue([ev = c.OSSIAEvent()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
      ev->cleanup();
    });
    c.cleanup(ev_it->second);
    return [&self, id = e.id()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      self.m_ossia_timeevents.erase(id);
    };
  }

  template <typename Base>
  static std::function<void()>
  removing_state(Base& self, const Scenario::StateModel& e, StateComponent& c)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    auto it = self.m_ossia_states.find(e.id());
    if(it == self.m_ossia_states.end())
      return {};
    c.onDelete();
    return [&self, id = e.id()] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      self.m_ossia_states.erase(id);
    };
  }

  template <typename Base>
  static IntervalComponent* make_interval(Base& self, Scenario::IntervalModel& cst)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    std::shared_ptr<ossia::scenario> proc
        = std::dynamic_pointer_cast<ossia::scenario>(self.m_ossia_process);

    auto elt = std::make_shared<IntervalComponent>(cst, proc, self.m_ctx, &self);
    self.m_ossia_intervals.insert({cst.id(), elt});

    auto& te = self.m_ossia_timeevents;
    SCORE_ASSERT(te.find(self.process().state(cst.startState()).eventId()) != te.end());
    SCORE_ASSERT(te.find(self.process().state(cst.endState()).eventId()) != te.end());
    auto ossia_sev = te.at(self.process().state(cst.startState()).eventId());
    auto ossia_eev = te.at(self.process().state(cst.endState()).eventId());

    auto dur = elt->makeDurations();
    constexpr auto cb = [](bool, ossia::time_value) {};
    auto ossia_cst = std::make_shared<ossia::time_interval>(
        smallfun::function<void(bool, ossia::time_value), 32>{cb},
        *ossia_sev->OSSIAEvent(), *ossia_eev->OSSIAEvent(),
        dur.defaultDuration, dur.minDuration, dur.maxDuration);

    ossia_cst->node->prepare(*self.m_ctx.execState);
    elt->onSetup(elt, ossia_cst, dur);

    const bool prop = cst.graphal() ? false : cst.outlet->propagate();
    self.system().executionQueue.enqueue(
        [g = self.system().execGraph, proc, ossia_sev, ossia_eev, ossia_cst, prop] {
          OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
          if(auto sev = ossia_sev->OSSIAEvent())
            sev->next_time_intervals().push_back(ossia_cst);
          if(auto eev = ossia_eev->OSSIAEvent())
            eev->previous_time_intervals().push_back(ossia_cst);
          proc->add_time_interval(ossia_cst);
          if(prop)
          {
            auto cable = g->allocate_edge(
                ossia::immediate_glutton_connection{},
                ossia_cst->node->root_outputs()[0],
                proc->node->root_inputs()[0], ossia_cst->node, proc->node);
            g->connect(cable);
          }
        });
    return elt.get();
  }

  template <typename Base>
  static StateComponent* make_state(Base& self, Scenario::StateModel& st)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    auto& events = self.m_ossia_timeevents;
    SCORE_ASSERT(events.find(st.eventId()) != events.end());
    auto ossia_ev = events.at(st.eventId());
    auto elt
        = std::make_shared<StateComponent>(st, ossia_ev->OSSIAEvent(), self.m_ctx, &self);
    self.m_ossia_states.insert({st.id(), elt});
    return elt.get();
  }

  // ExtraSetup: called with ossia_ev after onSetup. Sequence: [](auto&){}.
  // Scenario: connects timeSyncChanged signal.
  template <typename Base, typename ExtraSetup>
  static EventComponent*
  make_event(Base& self, Scenario::EventModel& ev, ExtraSetup&& extraSetup)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    auto elt = std::make_shared<EventComponent>(ev, self.m_ctx, &self);
    self.m_ossia_timeevents.insert({ev.id(), elt});

    auto& nodes = self.m_ossia_timesyncs;
    SCORE_ASSERT(nodes.find(ev.timeSync()) != nodes.end());
    auto tn = nodes.at(ev.timeSync());

    std::weak_ptr<EventComponent> weak_ev = elt;
    std::weak_ptr<Base> thisP
        = std::dynamic_pointer_cast<Base>(self.shared_from_this());
    auto ev_cb = [qed_ptr = self.system().weakEditionQueue(), weak_ev,
                  thisP](ossia::time_event::status st) {
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

    extraSetup(ossia_ev);

    self.m_ctx.executionQueue.enqueue(
        [event = ossia_ev, time_sync = tn->OSSIATimeSync()] {
          SCORE_ASSERT(event);
          SCORE_ASSERT(time_sync);
          OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
          time_sync->insert(time_sync->get_time_events().begin(), event);
        });

    return elt.get();
  }

  template <typename Base>
  static TimeSyncComponent* make_timesync(Base& self, Scenario::TimeSyncModel& tn)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    auto elt = std::make_shared<TimeSyncComponent>(tn, self.m_ctx, &self);
    self.m_ossia_timesyncs.insert({tn.id(), elt});

    bool must_add = false;
    std::shared_ptr<ossia::time_sync> ossia_tn;
    if(tn.id() == Scenario::startId<Scenario::TimeSyncModel>())
    {
      ossia_tn = self.OSSIAProcess().get_start_time_sync();
    }
    else
    {
      ossia_tn = std::make_shared<ossia::time_sync>();
      must_add = true;
    }

    elt->onSetup(ossia_tn, elt->makeTrigger());

    if(must_add)
    {
      auto ossia_sc = std::dynamic_pointer_cast<ossia::scenario>(self.m_ossia_process);
      self.m_ctx.executionQueue.enqueue([ossia_sc, ossia_tn] {
        OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
        ossia_sc->add_time_sync(ossia_tn);
      });
    }

    return elt.get();
  }

  template <typename Base>
  static void start_interval(Base& self, const Id<Scenario::IntervalModel>& id)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    auto& cst = self.process().intervals.at(id);
    if(self.m_executingIntervals.find(id) == self.m_executingIntervals.end())
      self.m_executingIntervals.insert(std::make_pair(cst.id(), &cst));
    auto it = self.m_ossia_intervals.find(id);
    if(it != self.m_ossia_intervals.end())
      it->second->executionStarted();
    cst.setExecutionState(Scenario::IntervalExecutionState::Enabled);
  }

  template <typename Base>
  static void disable_interval(Base& self, const Id<Scenario::IntervalModel>& id)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    self.process().intervals.at(id).setExecutionState(
        Scenario::IntervalExecutionState::Disabled);
  }

  template <typename Base>
  static void stop_interval(Base& self, const Id<Scenario::IntervalModel>& id)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    self.m_executingIntervals.erase(id);
    auto it = self.m_ossia_intervals.find(id);
    if(it != self.m_ossia_intervals.end())
      it->second->executionStopped();
  }

  // AfterStateUpdate: called per state after updating its event status.
  // Scenario: updates m_properties. Sequence: [](auto&){}.
  template <typename Base, typename AfterStateUpdate>
  static void event_callback(
      Base& self, std::weak_ptr<Base> selfWeak, std::shared_ptr<EventComponent> ev,
      ossia::time_event::status newStatus, AfterStateUpdate&& afterStateUpdate)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    if(!selfWeak.lock())
      return;
    auto the_event = const_cast<Scenario::EventModel*>(ev->scoreEvent());
    if(!the_event)
      return;

    the_event->setStatus(
        static_cast<Scenario::ExecutionStatus>(newStatus), self.process());

    for(auto& state : the_event->states())
    {
      auto& score_state = self.process().states.at(state);
      afterStateUpdate(score_state);

      switch(newStatus)
      {
        case ossia::time_event::status::NONE:
        case ossia::time_event::status::PENDING:
          break;
        case ossia::time_event::status::HAPPENED:
          if(score_state.previousInterval())
            self.stopIntervalExecution(*score_state.previousInterval());
          if(score_state.nextInterval())
            self.startIntervalExecution(*score_state.nextInterval());
          break;
        case ossia::time_event::status::DISPOSED:
          if(score_state.nextInterval())
            self.disableIntervalExecution(*score_state.nextInterval());
          break;
        default:
          SCORE_TODO;
          break;
      }
    }
  }
};

} // namespace Execution
