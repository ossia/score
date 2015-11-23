#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

namespace Scenario
{
template<typename Scenario_T>
class Transition_T<Scenario_T, ClickOnNothing> final :
        public MatchedTransition<Scenario_T, ClickOnNothing_Event>
{
    public:
        using MatchedTransition<Scenario_T, ClickOnNothing_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent *ev)
        {
            auto qev = static_cast<ClickOnNothing_Event*>(ev);
            this->state().clear();

            this->state().currentPoint = qev->point;
        }
};

template<typename Scenario_T>
using ClickOnNothing_Transition = Transition_T<Scenario_T, ClickOnNothing>;


template<typename Scenario_T>
class Transition_T<Scenario_T, MoveOnNothing> final:
        public MatchedTransition<Scenario_T, MoveOnNothing_Event>
{
    public:
        using MatchedTransition<Scenario_T, MoveOnNothing_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent *ev)
        {
            auto qev = static_cast<MoveOnNothing_Event*>(ev);

            this->state().currentPoint = qev->point;
        }
};
template<typename Scenario_T>
using MoveOnNothing_Transition = Transition_T<Scenario_T, MoveOnNothing>;


template<typename Scenario_T>
class Transition_T<Scenario_T, ReleaseOnNothing> final :
        public MatchedTransition<Scenario_T, ReleaseOnNothing_Event>
{
    public:
        using MatchedTransition<Scenario_T, ReleaseOnNothing_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent *ev)
        {
            auto qev = static_cast<ReleaseOnNothing_Event*>(ev);

            this->state().currentPoint = qev->point;
        }
};
template<typename Scenario_T>
using ReleaseOnNothing_Transition = Transition_T<Scenario_T, ReleaseOnNothing>;
}
