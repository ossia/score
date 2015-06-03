#pragma once
#include "CurveStateMachineBaseEvents.hpp"
#include "CurveStateMachineBaseStates.hpp"
namespace Curve
{
template<typename T>
using GenericCurveTransition = StateAwareTransition<StateBase, T>;

template<typename Event>
class MatchedCurveTransition : public GenericCurveTransition<MatchedTransition<Event>>
{
    public:
        using GenericCurveTransition<MatchedTransition<Event>>::GenericCurveTransition;
};


template<typename Element_T, typename Modifier_T>
class PositionedCurveTransition : public MatchedCurveTransition<CurveEvent<Element_T, Modifier_T>>
{
    public:
        using MatchedCurveTransition<CurveEvent<Element_T, Modifier_T>>::MatchedCurveTransition;

    protected:
        virtual void onTransition(QEvent * ev) override
        {
            auto qev = static_cast<CurveEvent<Element_T, Modifier_T>*>(ev);
            this->state().currentPoint = qev->point;

            impl(qev);
        }

    private:
        template<typename Element_Fun_T>
        void impl(CurveEvent<Element_Fun_T, Modifier::Click_tag>* ev)
        {
            this->state().clickedItem = ev->item;
        }

        template<typename Element_Fun_T>
        void impl(CurveEvent<Element_Fun_T, Modifier::Move_tag>* ev)
        {
            this->state().hoveredItem = ev->item;
        }

        template<typename Element_Fun_T>
        void impl(CurveEvent<Element_Fun_T, Modifier::Release_tag>* ev)
        {
            this->state().hoveredItem = ev->item;
        }
};

using ClickOnNothing_Transition = PositionedCurveTransition<Element::Nothing_tag, Modifier::Click_tag>;
using ClickOnPoint_Transition = PositionedCurveTransition<Element::Point_tag, Modifier::Click_tag>;
using ClickOnSegment_Transition = PositionedCurveTransition<Element::Segment_tag, Modifier::Click_tag>;

using MoveOnNothing_Transition = PositionedCurveTransition<Element::Nothing_tag, Modifier::Move_tag>;
using MoveOnPoint_Transition = PositionedCurveTransition<Element::Point_tag, Modifier::Move_tag>;
using MoveOnSegment_Transition = PositionedCurveTransition<Element::Segment_tag, Modifier::Move_tag>;

using ReleaseOnNothing_Transition = PositionedCurveTransition<Element::Nothing_tag, Modifier::Release_tag>;
using ReleaseOnPoint_Transition = PositionedCurveTransition<Element::Point_tag, Modifier::Release_tag>;
using ReleaseOnSegment_Transition = PositionedCurveTransition<Element::Segment_tag, Modifier::Release_tag>;
}
