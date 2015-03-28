#include "ScenarioExecutor.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Algorithms/Execution/ConstraintExecutor.hpp"

class EventExecutor
{
        EventModel& m_event;

    public:
        QList<ConstraintExecutor*> m_previousConstraints;
        QList<ConstraintExecutor*> m_nextConstraints;

        EventExecutor(EventModel& ev):
            m_event{ev}
        {

        }
        void addPreviousConstraint(ConstraintExecutor* c)
        { m_previousConstraints.push_back(c); }
        void addNextConstraint(ConstraintExecutor* c)
        { m_nextConstraints.push_back(c); }

        // Add executors of constraint reference
        EventModel& event()
        { return m_event; }

        bool check()
        {
            if(m_event.condition().isEmpty())
            {
                return true;
            }
            else
            {
                // check device and evaluate condition
                return false;
            }
        }
};

void ScenarioExecutor::start()
{
    // Mark all events as "false"
    // On each tick, check for each event if it is evaluable
    // For each evaluable event, evaluate it
    // If it is true, evaluate it and its whole timenode., and start the
    // following constraints

}

void ScenarioExecutor::stop()
{
}

void ScenarioExecutor::onTick(const TimeValue& time)
{
    using namespace std;
    auto events = m_events;
    for(auto& event : events)
    {
        // Check if the event is evaluable
        // All the previous constraints must be after their min duration
        bool ok = std::all_of(begin(event->m_previousConstraints),
                              end(event->m_previousConstraints),
                              [&] (ConstraintExecutor* ex) { return ex->evaluating(); });
        // If it is the case
        if(ok && event->check())
        {

            qDebug()<< "Starting event" << event->event().id();
            // Remove the previous constraints executors
            for(auto& cst : event->m_previousConstraints)
                m_constraints.removeAll(cst);

            // Execute the timenode
            for(auto& constraint : event->m_nextConstraints)
            {
                constraint->start();
            }
            m_events.removeAll(event);
            delete event;
        }
    }
}

ScenarioExecutor::ScenarioExecutor(ScenarioModel& model):
  m_model{model}
{
    using namespace std;
    for(auto& event : m_model.events())
    {
        m_events.push_back(new EventExecutor{*event});
    }

    for(auto& constraint : m_model.constraints())
    {
        auto executor = new ConstraintExecutor{*constraint};
        m_constraints.push_back(executor);

        // start event executor
        auto sev = find_if(begin(m_events), end(m_events), [&] (EventExecutor* ev) { return ev->event().id() == constraint->startEvent(); });
        (*sev)->addNextConstraint(executor);

        // end event executor
        auto eev = find_if(begin(m_events), end(m_events), [&] (EventExecutor* ev) { return ev->event().id() == constraint->endEvent(); });
        (*eev)->addPreviousConstraint(executor);
    }
}
