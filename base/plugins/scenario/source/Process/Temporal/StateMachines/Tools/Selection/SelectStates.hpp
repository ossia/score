#pragma once
#include <iscore/command/OngoingCommandManager.hpp>
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"

#include <iscore/selection/SelectionDispatcher.hpp>
class SelectConstraintState : public CommonState
{
    public:
        SelectConstraintState(iscore::SelectionDispatcher& dispatcher,
                              QState* parent);
};

class SelectEventState : public CommonState
{
    public:
        SelectEventState(iscore::SelectionDispatcher& dispatcher,
                              QState* parent);
};

class SelectTimeNodeState : public CommonState
{
    public:
        SelectTimeNodeState(iscore::SelectionDispatcher& dispatcher,
                              QState* parent);
};
