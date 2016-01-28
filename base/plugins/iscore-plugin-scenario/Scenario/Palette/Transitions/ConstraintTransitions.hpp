#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>

namespace Scenario
{
template<typename Scenario_T>
class Transition_T<Scenario_T, ClickOnConstraint> final :
        public MatchedTransition<Scenario_T, ClickOnConstraint_Event>
{
    public:
        using MatchedTransition<Scenario_T, ClickOnConstraint_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ClickOnConstraint_Event*>(ev);
            this->state().clear();

            this->state().clickedConstraint = qev->id;
            this->state().currentPoint = qev->point;
        }
};
template<typename Scenario_T>
using ClickOnConstraint_Transition = Transition_T<Scenario_T, ClickOnConstraint>;

template<typename Scenario_T>
class Transition_T<Scenario_T, MoveOnConstraint> final :
        public MatchedTransition<Scenario_T, MoveOnConstraint_Event>
{
    public:
        using MatchedTransition<Scenario_T, MoveOnConstraint_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<MoveOnConstraint_Event*>(ev);

            this->state().hoveredConstraint= qev->id;
            this->state().currentPoint = qev->point;
        }
};
template<typename Scenario_T>
using MoveOnConstraint_Transition = Transition_T<Scenario_T, MoveOnConstraint>;

template<typename Scenario_T>
class Transition_T<Scenario_T, ClickOnLeftBrace> final :
        public MatchedTransition<Scenario_T, ClickOnLeftBrace_Event>
{
    public:
        using MatchedTransition<Scenario_T, ClickOnLeftBrace_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ClickOnLeftBrace_Event*>(ev);
            this->state().clear();

            this->state().clickedConstraint = qev->id;
            this->state().currentPoint = qev->point;
        }
};
template<typename Scenario_T>
using ClickOnLeftBrace_Transition = Transition_T<Scenario_T, ClickOnLeftBrace>;

template<typename Scenario_T>
class Transition_T<Scenario_T, ClickOnRightBrace> final :
        public MatchedTransition<Scenario_T, ClickOnRightBrace_Event>
{
    public:
        using MatchedTransition<Scenario_T, ClickOnRightBrace_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ClickOnRightBrace_Event*>(ev);
            this->state().clear();

            this->state().clickedConstraint = qev->id;
            this->state().currentPoint = qev->point;
        }
};
template<typename Scenario_T>
using ClickOnRightBrace_Transition = Transition_T<Scenario_T, ClickOnRightBrace>;

} // namespace Scenario
