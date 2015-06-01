#pragma once
#include "ScenarioStateMachineBaseEvents.hpp"
#include "ScenarioStateMachineBaseStates.hpp"


using Press_Transition = MatchedTransition<Press_Event>;
using Move_Transition = MatchedTransition<Move_Event>;
using Release_Transition = MatchedTransition<Release_Event>;
using Cancel_Transition = MatchedTransition<Cancel_Event>;
using ShiftTransition = MatchedTransition<Shift_Event>;

// TODO rename in something like CreateMove transition...
// Or something that means that it will operate on constraint, event, etc...
template<typename T>
class GenericTransition : public T
{
    public:
        GenericTransition(CommonScenarioState& state):
                    m_state{state} { }

        CommonScenarioState& state() const { return m_state; }

    private:
        CommonScenarioState& m_state;
};

template<typename Event>
class MatchedScenarioTransition : public GenericTransition<MatchedTransition<Event>>
{
    public:
        using GenericTransition<MatchedTransition<Event>>::GenericTransition;
};
