#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
namespace Scenario
{
template<typename Scenario_T>
class Transition_T<Scenario_T, ClickOnState> final :
        public MatchedTransition<Scenario_T, ClickOnState_Event>
{
    public:
        using MatchedTransition<Scenario_T, ClickOnState_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ClickOnState_Event*>(ev);
            this->state().clear();

            this->state().clickedState= qev->id;
            this->state().currentPoint = qev->point;
        }
};

template<typename Scenario_T>
using ClickOnState_Transition = Transition_T<Scenario_T, ClickOnState>;


template<typename Scenario_T>
class ClickOnEndState_Transition final :
        public MatchedTransition<Scenario_T, ClickOnState_Event>
{
    public:
        using MatchedTransition<Scenario_T, ClickOnState_Event>::MatchedTransition;

    protected:
        bool eventTest(QEvent* e) override
        {
            if(e->type() == QEvent::Type(QEvent::User + ClickOnState_Event::user_type))
            {
                auto qev = static_cast<ClickOnState_Event*>(e);
                return qev->id == Scenario::endId<StateModel>();
            }
            return false;
        }

        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ClickOnState_Event*>(ev);
            this->state().clear();

            this->state().clickedState= qev->id;
            this->state().currentPoint = qev->point;
        }
};

template<typename Scenario_T>
class Transition_T<Scenario_T, MoveOnState> final :
        public MatchedTransition<Scenario_T, MoveOnState_Event>
{
    public:
        using MatchedTransition<Scenario_T, MoveOnState_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<MoveOnState_Event*>(ev);

            this->state().hoveredState = qev->id;
            this->state().currentPoint = qev->point;
        }
};

template<typename Scenario_T>
using MoveOnState_Transition = Transition_T<Scenario_T, MoveOnState>;


template<typename Scenario_T>
class Transition_T<Scenario_T, ReleaseOnState> final :
        public MatchedTransition<Scenario_T, ReleaseOnState_Event>
{
    public:
        using MatchedTransition<Scenario_T, ReleaseOnState_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ReleaseOnState_Event*>(ev);

            this->state().hoveredState = qev->id;
            this->state().currentPoint = qev->point;
        }
};

template<typename Scenario_T>
using ReleaseOnState_Transition = Transition_T<Scenario_T, ReleaseOnState>;
}
