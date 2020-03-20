#pragma once
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <ossia/editor/curve/curve.hpp>

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

// TODO
template <>
struct CurveTraits<double>
{
  static const constexpr auto fun = &Curve::SegmentModel::makeDoubleFunction;
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

  for (const auto& score_segment : segments)
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

// Simpler curve, between [0; 1]
template <typename Segments>
ossia::curve<double, float> floatCurve(
    const Segments& segments,
    const optional<ossia::destination>& tween)
{
  ossia::curve<double, float> curve;

  auto start = segments[0]->start();
  if (start.x() == 0.)
  {
    curve.set_x0(start.x());
    curve.set_y0(start.y());
  }

  for (const auto& score_segment : segments)
  {
    auto end = score_segment->end();
    curve.add_point(
        (score_segment->*CurveTraits<float>::fun)(),
        end.x(),
        end.y());
  }

  if (tween)
  {
    curve.set_y0_destination(*tween);
  }

  return curve;
}

}
}
