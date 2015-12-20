#pragma once
#include <iscore/tools/SettableIdentifier.hpp>

class ConstraintModel;
class StateModel;
class Process;
void AddProcess(ConstraintModel& constraint, Process*);
void RemoveProcess(ConstraintModel& constraint, const Id<Process>&);

void SetPreviousConstraint(StateModel& state, const ConstraintModel& constraint);
void SetNextConstraint(StateModel& state, const ConstraintModel& constraint);
void SetNoPreviousConstraint(StateModel& state);
void SetNoNextConstraint(StateModel& state);
