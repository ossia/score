#pragma once
#include "CurvePoint.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <iscore/tools/Clamp.hpp>

class QQuickPaintedItem;
namespace Curve
{
class SegmentModel;
namespace Element
{
struct Nothing_tag
{
  static constexpr const int value = 0;
};
struct Point_tag
{
  static constexpr const int value = 1;
};
struct Segment_tag
{
  static constexpr const int value = 2;
};
}

template <typename Element_T, typename Modifier_T>
struct CurveEvent : public iscore::PositionedEvent<Curve::Point>
{
  static constexpr const int user_type = Element_T::value + Modifier_T::value;
  CurveEvent(const Curve::Point& pt, const QQuickPaintedItem* theItem)
      : iscore::PositionedEvent<Curve::Point>{pt,
                                              QEvent::Type(
                                                  QEvent::User + user_type)}
      , item{theItem}
  {
  }

  const QQuickPaintedItem* const item{};
};

using ClickOnNothing_Event
    = CurveEvent<Element::Nothing_tag, iscore::Modifier::Click_tag>;
using ClickOnPoint_Event
    = CurveEvent<Element::Point_tag, iscore::Modifier::Click_tag>;
using ClickOnSegment_Event
    = CurveEvent<Element::Segment_tag, iscore::Modifier::Click_tag>;

using MoveOnNothing_Event
    = CurveEvent<Element::Nothing_tag, iscore::Modifier::Move_tag>;
using MoveOnPoint_Event
    = CurveEvent<Element::Point_tag, iscore::Modifier::Move_tag>;
using MoveOnSegment_Event
    = CurveEvent<Element::Segment_tag, iscore::Modifier::Move_tag>;

using ReleaseOnNothing_Event
    = CurveEvent<Element::Nothing_tag, iscore::Modifier::Release_tag>;
using ReleaseOnPoint_Event
    = CurveEvent<Element::Point_tag, iscore::Modifier::Release_tag>;
using ReleaseOnSegment_Event
    = CurveEvent<Element::Segment_tag, iscore::Modifier::Release_tag>;
}
