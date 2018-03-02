#pragma once
#include <ossia/editor/curve/curve.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

namespace Engine
{
namespace score_to_ossia
{

template <typename Y_T>
struct CurveTraits;

template <>
struct CurveTraits<int>
{
  static const constexpr auto fun = &Curve::SegmentModel::makeIntFunction;
};

template <>
struct CurveTraits<float>
{
  static const constexpr auto fun = &Curve::SegmentModel::makeFloatFunction;
};

template <>
struct CurveTraits<bool>
{
  static const constexpr auto fun = &Curve::SegmentModel::makeBoolFunction;
};

template <
    typename X_T,
    typename Y_T,
    typename XScaleFun,
    typename YScaleFun,
    typename Segments>
std::shared_ptr<ossia::curve<X_T, Y_T>> curve(
    XScaleFun scale_x,
    YScaleFun scale_y,
    const Segments& segments,
    const optional<ossia::destination>& tween)
{
  auto curve = std::make_shared<ossia::curve<X_T, Y_T>>();

  auto start = segments[0]->start();
  if (start.x() == 0.)
  {
    curve->set_x0(scale_x(start.x()));
    curve->set_y0(scale_y(start.y()));
  }

  for (auto score_segment : segments)
  {
      auto end = score_segment->end();
      curve->add_point(
        (score_segment->*CurveTraits<Y_T>::fun)(),
        scale_x(end.x()),
        scale_y(end.y()));
  }

  if (tween)
  {
    curve->set_y0_destination(*tween);
  }

  return curve;
}


template <
    typename X_T,
    typename Y_T,
    typename XScaleFun,
    typename Segments>
std::shared_ptr<ossia::curve_abstract> scalable_curve(
    Y_T min, Y_T max, Y_T end,
    XScaleFun scale_x,
    const Segments& segments,
    const ossia::destination& tween)
{
  auto curve = std::make_shared<ossia::curve<X_T, Y_T>>();

  auto start = segments[0]->start();
  if (start.x() == 0.)
  {
    curve->set_x0(scale_x(start.x()));
    curve->set_y0(start.y());
  }

  curve->set_scale_bounds(min, max, end);
  curve->set_y0_destination(tween);

  for (auto score_segment : segments)
  {
    auto end = score_segment->end();
    curve->add_point(
        (score_segment->*CurveTraits<Y_T>::fun)(),
          scale_x(end.x()), end.y());
  }

  return curve; 
}
}
}
