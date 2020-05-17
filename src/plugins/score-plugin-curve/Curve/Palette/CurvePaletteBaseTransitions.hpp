#pragma once
#include "CurvePaletteBaseEvents.hpp"
#include "CurvePaletteBaseStates.hpp"

#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>

namespace Curve
{
// TODO cleanup this file

template <typename T>
using GenericTransition = score::StateAwareTransition<StateBase, T>;

template <typename Event>
class MatchedCurveTransition : public GenericTransition<score::MatchedTransition<Event>>
{
public:
  using GenericTransition<score::MatchedTransition<Event>>::GenericTransition;
};

template <typename Element_T, typename Modifier_T>
class PositionedCurveTransition final
    : public MatchedCurveTransition<CurveEvent<Element_T, Modifier_T>>
{
public:
  using MatchedCurveTransition<CurveEvent<Element_T, Modifier_T>>::MatchedCurveTransition;

protected:
  void onTransition(QEvent* ev) override
  {
    auto qev = safe_cast<CurveEvent<Element_T, Modifier_T>*>(ev);
    this->state().currentPoint = qev->point;

    impl(qev);
  }

private:
  void impl(CurveEvent<Element::Nothing_tag, score::Modifier::Click_tag>* ev) { }
  void impl(CurveEvent<Element::Point_tag, score::Modifier::Click_tag>* ev)
  {
    auto& model = safe_cast<const PointView*>(ev->item)->model();
    this->state().clickedPointId = {model.previous(), model.following()};
  }
  void impl(CurveEvent<Element::Segment_tag, score::Modifier::Click_tag>* ev)
  {
    this->state().clickedSegmentId = safe_cast<const SegmentView*>(ev->item)->model().id();
  }

  void impl(CurveEvent<Element::Nothing_tag, score::Modifier::Move_tag>* ev) { }
  void impl(CurveEvent<Element::Point_tag, score::Modifier::Move_tag>* ev)
  {
    auto& model = safe_cast<const PointView*>(ev->item)->model();
    this->state().hoveredPointId = {model.previous(), model.following()};
  }
  void impl(CurveEvent<Element::Segment_tag, score::Modifier::Move_tag>* ev)
  {
    this->state().hoveredSegmentId = safe_cast<const SegmentView*>(ev->item)->model().id();
  }

  void impl(CurveEvent<Element::Nothing_tag, score::Modifier::Release_tag>* ev) { }
  void impl(CurveEvent<Element::Point_tag, score::Modifier::Release_tag>* ev)
  {
    auto& model = safe_cast<const PointView*>(ev->item)->model();
    this->state().hoveredPointId = {model.previous(), model.following()};
  }
  void impl(CurveEvent<Element::Segment_tag, score::Modifier::Release_tag>* ev)
  {
    this->state().hoveredSegmentId = safe_cast<const SegmentView*>(ev->item)->model().id();
  }
};

using ClickOnNothing_Transition
    = PositionedCurveTransition<Element::Nothing_tag, score::Modifier::Click_tag>;
using ClickOnPoint_Transition
    = PositionedCurveTransition<Element::Point_tag, score::Modifier::Click_tag>;
using ClickOnSegment_Transition
    = PositionedCurveTransition<Element::Segment_tag, score::Modifier::Click_tag>;

using MoveOnNothing_Transition
    = PositionedCurveTransition<Element::Nothing_tag, score::Modifier::Move_tag>;
using MoveOnPoint_Transition
    = PositionedCurveTransition<Element::Point_tag, score::Modifier::Move_tag>;
using MoveOnSegment_Transition
    = PositionedCurveTransition<Element::Segment_tag, score::Modifier::Move_tag>;

using ReleaseOnNothing_Transition
    = PositionedCurveTransition<Element::Nothing_tag, score::Modifier::Release_tag>;
using ReleaseOnPoint_Transition
    = PositionedCurveTransition<Element::Point_tag, score::Modifier::Release_tag>;
using ReleaseOnSegment_Transition
    = PositionedCurveTransition<Element::Segment_tag, score::Modifier::Release_tag>;

class ClickOnAnything_Transition final : public GenericTransition<QAbstractTransition>
{
public:
  using GenericTransition<QAbstractTransition>::GenericTransition;

protected:
  bool eventTest(QEvent* e) override
  {
    using namespace std;
    static const constexpr QEvent::Type types[]
        = {QEvent::Type(QEvent::User + ClickOnNothing_Event::user_type),
           QEvent::Type(QEvent::User + ClickOnPoint_Event::user_type),
           QEvent::Type(QEvent::User + ClickOnSegment_Event::user_type)};

    return find(begin(types), end(types), e->type()) != end(types);
  }

  void onTransition(QEvent* e) override
  {
    auto qev = safe_cast<score::PositionedEvent<Curve::Point>*>(e);

    this->state().currentPoint = qev->point;
  }
};

class MoveOnAnything_Transition final : public GenericTransition<QAbstractTransition>
{
public:
  using GenericTransition<QAbstractTransition>::GenericTransition;

protected:
  bool eventTest(QEvent* e) override
  {
    using namespace std;
    static const constexpr QEvent::Type types[]
        = {QEvent::Type(QEvent::User + MoveOnNothing_Event::user_type),
           QEvent::Type(QEvent::User + MoveOnPoint_Event::user_type),
           QEvent::Type(QEvent::User + MoveOnSegment_Event::user_type)};

    return find(begin(types), end(types), e->type()) != end(types);
  }

  void onTransition(QEvent* e) override
  {
    auto qev = safe_cast<score::PositionedEvent<Curve::Point>*>(e);

    this->state().currentPoint = qev->point;
  }
};

class ReleaseOnAnything_Transition final : public QAbstractTransition
{
protected:
  bool eventTest(QEvent* e) override
  {
    using namespace std;
    static const constexpr QEvent::Type types[]
        = {QEvent::Type(QEvent::User + ReleaseOnNothing_Event::user_type),
           QEvent::Type(QEvent::User + ReleaseOnPoint_Event::user_type),
           QEvent::Type(QEvent::User + ReleaseOnSegment_Event::user_type)};

    return find(begin(types), end(types), e->type()) != end(types);
  }

  void onTransition(QEvent* e) override { }
};
}
