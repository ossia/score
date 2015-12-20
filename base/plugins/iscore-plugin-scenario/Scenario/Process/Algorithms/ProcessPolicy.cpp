#include "ProcessPolicy.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>

void AddProcess(ConstraintModel& constraint, Process* proc)
{
    constraint.processes.add(proc);
}

void RemoveProcess(ConstraintModel& constraint, const Id<Process>& proc)
{
    constraint.processes.remove(proc);
}

void SetPreviousConstraint(StateModel& state, const ConstraintModel& constraint)
{
    state.setPreviousConstraint(constraint.id());
}

void SetNextConstraint(StateModel& state, const ConstraintModel& constraint)
{
    state.setNextConstraint(constraint.id());
}

void SetNoPreviousConstraint(StateModel& state)
{
    state.setPreviousConstraint(Id<ConstraintModel>{});
}

void SetNoNextConstraint(StateModel& state)
{
    state.setNextConstraint(Id<ConstraintModel>{});
}
