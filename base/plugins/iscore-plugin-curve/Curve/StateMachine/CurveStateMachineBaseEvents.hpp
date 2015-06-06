#pragma once
#include "CurvePoint.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <iscore/tools/Clamp.hpp>

class CurveSegmentModel;
class QGraphicsItem;
namespace Curve
{
    namespace Element {
        struct Nothing_tag{ static constexpr const int value = 0; };
        struct Point_tag  { static constexpr const int value = 1; };
        struct Segment_tag{ static constexpr const int value = 2; };
    }

template<typename Element_T, typename Modifier_T>
struct CurveEvent : public PositionedEvent<CurvePoint>
{
        static constexpr const int user_type = Element_T::value + Modifier_T::value;
        CurveEvent(const CurvePoint& pt, const QGraphicsItem* theItem):
            PositionedEvent<CurvePoint>{
                pt,
                QEvent::Type(QEvent::User + user_type)},
            item{theItem}
        {
        }

        const QGraphicsItem* const item{};
};

using ClickOnNothing_Event = CurveEvent<Element::Nothing_tag, Modifier::Click_tag>;
using ClickOnPoint_Event   = CurveEvent<Element::Point_tag,   Modifier::Click_tag>;
using ClickOnSegment_Event = CurveEvent<Element::Segment_tag, Modifier::Click_tag>;

using MoveOnNothing_Event = CurveEvent<Element::Nothing_tag, Modifier::Move_tag>;
using MoveOnPoint_Event   = CurveEvent<Element::Point_tag,   Modifier::Move_tag>;
using MoveOnSegment_Event = CurveEvent<Element::Segment_tag, Modifier::Move_tag>;

using ReleaseOnNothing_Event = CurveEvent<Element::Nothing_tag, Modifier::Release_tag>;
using ReleaseOnPoint_Event   = CurveEvent<Element::Point_tag,   Modifier::Release_tag>;
using ReleaseOnSegment_Event = CurveEvent<Element::Segment_tag, Modifier::Release_tag>;
}
