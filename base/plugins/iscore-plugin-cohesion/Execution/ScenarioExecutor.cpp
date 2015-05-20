#include "ScenarioExecutor.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "ConstraintExecutor.hpp"
#include <Plugin/DeviceExplorerPlugin.hpp>

#include <set>


void executeState(const State& state)
{
    if(state.data().canConvert<MessageList>())
    {
        auto ml = state.data().value<MessageList>();
        /*
        for(auto& message : ml)
        {
            SingletonDeviceList::sendMessage(message);
        }
        */
    }
    else if(state.data().canConvert<Message>())
    {
        //SingletonDeviceList::sendMessage(state.data().value<Message>());
    }
    else if(state.data().canConvert<StateList>())
    {
        for(auto& substate : state.data().value<StateList>())
        {
            executeState(substate);
        }
    }
    else if(state.data().canConvert<State>())
    {
        executeState(state.data().value<State>());
    }

}


class EventExecutor
{
        EventModel& m_event;

    public:
        QSet<ConstraintExecutor*> m_previousConstraints;
        QSet<ConstraintExecutor*> m_nextConstraints;

        TimeNodeExecutor* m_timeNode;

        bool disabled() const
        {
            return (m_previousConstraints.size() > 0 &&
                   std::all_of(std::begin(m_previousConstraints),
                               std::end(m_previousConstraints),
                               [] (ConstraintExecutor* cst) { return cst->disabled(); }));
        }
        EventExecutor(EventModel& ev):
            m_event{ev}
        {

        }

        EventModel& event()
        { return m_event; }

        bool check()
        {
            using namespace std;
            if(m_event.condition().isEmpty() /*|| SingletonDeviceList::check(m_event.condition())*/) // TODO check here
            {
                if(m_previousConstraints.empty())
                {
                    return true;
                }
                else
                {
                    std::set<ConstraintExecutor*> nonDisabled;
                    copy_if(begin(m_previousConstraints), end(m_previousConstraints),
                            std::inserter(nonDisabled, nonDisabled.begin()),
                            [ ] (ConstraintExecutor* ex) { return !ex->disabled();});

                    if(nonDisabled.empty())
                    {
                        return false;
                    }
                    else
                    {
                        return all_of(
                            begin(nonDisabled),
                            end(nonDisabled),
                            [&] (ConstraintExecutor* ex) { return ex->evaluating(); });
                    }
                }
            }
            return false;
        }
};

class TimeNodeExecutor
{
        TimeNodeModel& m_timenode;

    public:
        QSet<EventExecutor*> m_events;
        TimeNodeExecutor(TimeNodeModel& tn):
            m_timenode{tn}
        {

        }

        void execute()
        {
            for(EventExecutor* event : m_events)
            {
                // 1. stop and remove all the previous constraints.
                // The CSP must enforce the correct min / max bounds.
                for(ConstraintExecutor* prev_cst : event->m_previousConstraints)
                {
                    prev_cst->stop();
                }

                // 2. Check which event has its condition true
                if(event->check())
                {
                    // Send the states of the event
                    for(const State& state : event->event().states())
                    {
                        executeState(state);
                    }

                    // Start the constraints
                    for(ConstraintExecutor* next_cst : event->m_nextConstraints)
                    {
                        next_cst->start();
                    }
                }
                else
                {
                    for(ConstraintExecutor* next_cst : event->m_nextConstraints)
                    {
                        next_cst->disable();
                    }
                }
            }
        }
};

void ScenarioExecutor::start()
{
    using namespace std;
    EventExecutor* eev = *find_if(begin(m_events), end(m_events), [&] (EventExecutor* ev) { return ev->event().id() == id_type<EventModel>(0); });
    eev->m_timeNode->execute();
}

void ScenarioExecutor::stop()
{
    for(auto& constraint : m_constraints)
        constraint->stop();
}

void ScenarioExecutor::onTick(const TimeValue& )
{
    using namespace std;
    auto events = m_events;

    // First get all the constraints that are in their evaluation zone
    QSet<EventExecutor*> eventsToCheck;
    for(auto& constraint : m_constraints)
    {
        if(constraint->constraint().playDuration() >= constraint->constraint().minDuration())
        {
            auto eev = find_if(begin(m_events), end(m_events), [&] (EventExecutor* ev) { return ev->event().id() == constraint->constraint().endEvent(); });
            eventsToCheck.insert(*eev);
        }
    }

    QSet<TimeNodeExecutor*> executingTimenodes;
    for(EventExecutor* event : eventsToCheck)
    {
        if(event->check() && (event->event().id() != id_type<EventModel>(1)))
        {
            executingTimenodes.insert(event->m_timeNode);
        }
    }

    for(TimeNodeExecutor* tn : executingTimenodes)
    {
        tn->execute();
    }

    for(TimeNodeExecutor* tn : executingTimenodes)
    {
        // Cleanup
        for(auto& event : tn->m_events)
        {
            for(auto& prev_cst : event->m_previousConstraints)
            {
                m_constraints.remove(prev_cst);
                delete prev_cst;
            }

            m_events.remove(event);
            delete event;
        }
        m_timenodes.remove(tn);
        delete tn;
    }

}

ScenarioExecutor::ScenarioExecutor(ScenarioModel& model):
  m_model{model}
{
    using namespace std;
    for(auto& event : m_model.events())
    {
        m_events.insert(new EventExecutor{*event});
    }

    for(auto& constraint : m_model.constraints())
    {
        auto executor = new ConstraintExecutor{*constraint};
        m_constraints.insert(executor);

        // start event executor
        auto sev = find_if(begin(m_events), end(m_events), [&] (EventExecutor* ev) { return ev->event().id() == constraint->startEvent(); });
        (*sev)->m_nextConstraints.insert(executor);

        // end event executor
        auto eev = find_if(begin(m_events), end(m_events), [&] (EventExecutor* ev) { return ev->event().id() == constraint->endEvent(); });
        (*eev)->m_previousConstraints.insert(executor);
    }

    for(auto& timenode : m_model.timeNodes())
    {
        auto tn = new TimeNodeExecutor{*timenode};
        m_timenodes.insert(tn);

        for(auto& tn_event : timenode->events())
        {
            auto sev = find_if(begin(m_events), end(m_events), [&] (EventExecutor* ev) { return ev->event().id() == tn_event; });
            tn->m_events.insert(*sev);
            (*sev)->m_timeNode = tn;
        }
    }
}
