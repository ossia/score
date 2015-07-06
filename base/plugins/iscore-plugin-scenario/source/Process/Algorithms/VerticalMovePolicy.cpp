#include "VerticalMovePolicy.hpp"

#include <Process/ScenarioModel.hpp>
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>


void updateEventExtent(const id_type<EventModel> &id, ScenarioModel &s)
{
    auto& ev = s.event(id);
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::min();
    for(const auto& state_id : ev.states())
    {
        const auto& st = s.state(state_id);
        if(st.heightPercentage() < min)
            min = st.heightPercentage();
        if(st.heightPercentage() > max)
            max = st.heightPercentage();
    }

    ev.setExtent({{min, max}});
    updateTimeNodeExtent(ev.timeNode(), s);
}


void updateTimeNodeExtent(const id_type<TimeNodeModel> &id, ScenarioModel &s)
{
    auto& tn = s.timeNode(id);
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::min();
    for(const auto& ev_id : tn.events())
    {
        const auto& ev = s.event(ev_id);
        if(ev.extent().top() < min)
            min = ev.extent().top();
        if(ev.extent().bottom() > max)
            max = ev.extent().bottom();
    }

    tn.setExtent({{min, max}});
    // TODO what happens if no event on timenode / no state on event??
}

void updateConstraintVerticalPos(double y, const id_type<ConstraintModel> &id, ScenarioModel &s)
{
    auto& cst = s.constraint(id);

    // First make the list of all the constraints to update
    QSet<ConstraintModel*> constraintsToUpdate;
    constraintsToUpdate.insert(&cst);
    QSet<StateModel*> statesToUpdate;
    StateModel* rec_state = &s.state(cst.startState());
    while(rec_state->previousConstraint())
    {
        ConstraintModel* rec_cst = &s.constraint(rec_state->previousConstraint());
        constraintsToUpdate.insert(rec_cst);
        statesToUpdate.insert(rec_state);
        rec_state = &s.state(rec_cst->startState());
    }
    statesToUpdate.insert(rec_state); // Add the first state

    rec_state = &s.state(cst.endState());
    while(rec_state->nextConstraint())
    {
        ConstraintModel* rec_cst = &s.constraint(rec_state->nextConstraint());
        constraintsToUpdate.insert(rec_cst);
        statesToUpdate.insert(rec_state);
        rec_state = &s.state(rec_cst->endState());
    }
    statesToUpdate.insert(rec_state); // Add the last state

    // Set the correct height
    for(auto& constraint : constraintsToUpdate)
    {
        constraint->setHeightPercentage(y);
    }

    for(auto& state : statesToUpdate)
    {
        state->setHeightPercentage(y);
        updateEventExtent(state->eventId(), s);
    }

}
