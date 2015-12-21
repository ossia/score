#pragma once
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
namespace Scenario
{
template<typename Scenario_T>
class Transition_T<Scenario_T, ClickOnTimeNode> final :
        public MatchedTransition<Scenario_T, ClickOnTimeNode_Event>
{
    public:
        using MatchedTransition<Scenario_T, ClickOnTimeNode_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ClickOnTimeNode_Event*>(ev);
            this->state().clear();

            this->state().clickedTimeNode = qev->id;
            this->state().currentPoint = qev->point;
        }

};
template<typename Scenario_T>
using ClickOnTimeNode_Transition = Transition_T<Scenario_T, ClickOnTimeNode>;

template<typename Scenario_T>
class ClickOnEndTimeNode_Transition final :
        public MatchedTransition<Scenario_T, ClickOnTimeNode_Event>
{
    public:
        using MatchedTransition<Scenario_T, ClickOnTimeNode_Event>::MatchedTransition;

    protected:
        bool eventTest(QEvent* e) override
        {
            if(e->type() == QEvent::Type(QEvent::User + ClickOnTimeNode_Event::user_type))
            {
                auto qev = static_cast<ClickOnTimeNode_Event*>(e);
                return qev->id == Scenario::endId<TimeNodeModel>();
            }
            return false;
        }

        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ClickOnTimeNode_Event*>(ev);
            this->state().clear();

            this->state().clickedTimeNode = qev->id;
            this->state().currentPoint = qev->point;
        }
};


template<typename Scenario_T>
class Transition_T<Scenario_T, MoveOnTimeNode> final :
        public MatchedTransition<Scenario_T, MoveOnTimeNode_Event>
{
    public:
        using MatchedTransition<Scenario_T, MoveOnTimeNode_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<MoveOnTimeNode_Event*>(ev);

            this->state().hoveredTimeNode = qev->id;
            this->state().currentPoint = qev->point;
        }
};
template<typename Scenario_T>
using MoveOnTimeNode_Transition = Transition_T<Scenario_T, MoveOnTimeNode>;


template<typename Scenario_T>
class Transition_T<Scenario_T, ReleaseOnTimeNode> final :
        public MatchedTransition<Scenario_T, ReleaseOnTimeNode_Event>
{
    public:
        using MatchedTransition<Scenario_T, ReleaseOnTimeNode_Event>::MatchedTransition;

    protected:
        void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<ReleaseOnTimeNode_Event*>(ev);

            this->state().hoveredTimeNode = qev->id;
            this->state().currentPoint = qev->point;
        }
};
template<typename Scenario_T>
using ReleaseOnTimeNode_Transition = Transition_T<Scenario_T, ReleaseOnTimeNode>;

}
