#pragma once
#include "CurvePoint.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <iscore/tools/Clamp.hpp>
class CurveSegmentModel;


namespace Curve
{

struct PositionedEventBase : public QEvent
{
        PositionedEventBase(
                const CurvePoint& pt,
                QEvent::Type type):
            QEvent{type},
            point{pt}
        {
            // Here we artificially prevent to move over the header of the box
            // so that the elements won't disappear in the void.
            point.y = clamp(point.y, 0.0, 1.0);
        }

        CurvePoint point;
};

// We avoid virtual inheritance (with Numbered event);
// this replicates a tiny bit of code.
template<int N>
struct PositionedEvent : public PositionedEventBase
{
        static constexpr const int user_type = N;
        PositionedEvent(
                const CurvePoint& pt):
            PositionedEventBase{pt, QEvent::Type(QEvent::User + N)}
        {
        }
};

template<typename Element, int N>
struct PositionedWithId_Event : public PositionedEvent<N>
{
        PositionedWithId_Event(
                const id_type<Element>& theId,
                const CurvePoint& sp):
            PositionedEvent<N>{sp},
            id{theId}
        {
        }

        id_type<Element> id;
};


////////////
// Events
enum Element {
    Nothing, Point, Segment
};

using ClickOnNothing_Event = PositionedEvent<Element::Nothing + Modifier::Click>;
using ClickOnPoint_Event = PositionedEvent<Element::Point + Modifier::Click>;
using ClickOnSegment_Event = PositionedWithId_Event<CurveSegmentModel, Element::Segment + Modifier::Click>;

using MoveOnNothing_Event = PositionedEvent<Element::Nothing + Modifier::Move>;
using MoveOnPoint_Event = PositionedEvent<Element::Point + Modifier::Move>;
using MoveOnSegment_Event = PositionedWithId_Event<CurveSegmentModel, Element::Segment + Modifier::Move>;

using ReleaseOnNothing_Event = PositionedEvent<Element::Nothing + Modifier::Release>;
using ReleaseOnPoint_Event = PositionedEvent<Element::Point + Modifier::Release>;
using ReleaseOnSegment_Event = PositionedWithId_Event<CurveSegmentModel, Element::Segment + Modifier::Release>;
}
