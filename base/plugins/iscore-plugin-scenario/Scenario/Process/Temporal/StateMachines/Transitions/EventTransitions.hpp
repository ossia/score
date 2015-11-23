#pragma once
#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp>

namespace Scenario
{
template<typename Scenario_T>
class Transition_T<Scenario_T, ClickOnEvent> final :
        public MatchedTransition<Scenario_T, ClickOnEvent_Event>
{
    public:
        using MatchedTransition<Scenario_T, ClickOnEvent_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ClickOnEvent_Event*>(ev);
            this->state().clear();

            this->state().clickedEvent = qev->id;
            this->state().currentPoint = qev->point;
        }

};
template<typename Scenario_T>
using ClickOnEvent_Transition = Transition_T<Scenario_T, ClickOnEvent>;


template<typename Scenario_T>
class Transition_T<Scenario_T, MoveOnEvent> final :
        public MatchedTransition<Scenario_T, MoveOnEvent_Event>
{
    public:
        using MatchedTransition<Scenario_T, MoveOnEvent_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<MoveOnEvent_Event*>(ev);

            this->state().hoveredEvent = qev->id;
            this->state().currentPoint = qev->point;
        }

};
template<typename Scenario_T>
using MoveOnEvent_Transition = Transition_T<Scenario_T, MoveOnEvent>;


template<typename Scenario_T>
class Transition_T<Scenario_T, ReleaseOnEvent> final :
        public MatchedTransition<Scenario_T, ReleaseOnEvent_Event>
{
    public:
        using MatchedTransition<Scenario_T, ReleaseOnEvent_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ReleaseOnEvent_Event*>(ev);

            this->state().hoveredEvent = qev->id;
            this->state().currentPoint = qev->point;
        }
};
template<typename Scenario_T>
using ReleaseOnEvent_Transition = Transition_T<Scenario_T, ReleaseOnEvent>;
}
