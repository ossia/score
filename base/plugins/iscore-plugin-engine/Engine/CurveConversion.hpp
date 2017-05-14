#pragma once
#include <ossia/editor/curve/curve.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

namespace Engine
{
namespace iscore_to_ossia
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
std::shared_ptr<ossia::curve_abstract> curve(
    XScaleFun scale_x,
    YScaleFun scale_y,
    const Segments& segments,
    const optional<ossia::Destination>& tween)
{
  auto curve = std::make_shared<ossia::curve<X_T, Y_T>>();

  auto start = segments[0]->start();
  if (start.x() == 0.)
  {
    curve->set_x0(scale_x(start.x()));
    curve->set_y0(scale_y(start.y()));
  }

  for (auto iscore_segment : segments)
  {
    auto end = iscore_segment->end();
#if defined(_MSC_VER)
    if (std::is_same<Y_T, int>::value)
    {
      curve->add_point(
          iscore_segment->makeIntFunction(),
          scale_x(end.x()),
          scale_y(end.y()));
    }
    else if (std::is_same<Y_T, float>::value)
    {
      curve->add_point(
          iscore_segment->makeFloatFunction(),
          scale_x(end.x()),
          scale_y(end.y()));
    }
    else if (std::is_same<Y_T, bool>::value)
    {
      curve->add_point(
          iscore_segment->makeBoolFunction(),
          scale_x(end.x()),
          scale_y(end.y()));
    }
#else
    curve->add_point(
        (iscore_segment->*CurveTraits<Y_T>::fun)(),
        scale_x(end.x()),
        scale_y(end.y()));
#endif
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
    const ossia::Destination& tween)
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

  for (auto iscore_segment : segments)
  {
    auto end = iscore_segment->end();
#if defined(_MSC_VER)
    if (std::is_same<Y_T, int>::value)
    {
      curve->add_point(
          iscore_segment->makeIntFunction(),
          scale_x(end.x()), end.y());
    }
    else if (std::is_same<Y_T, float>::value)
    {
      curve->add_point(
          iscore_segment->makeFloatFunction(),
            scale_x(end.x()), end.y());
    }
    else if (std::is_same<Y_T, bool>::value)
    {
      curve->add_point(
          iscore_segment->makeBoolFunction(),
            scale_x(end.x()), end.y());
    }
#else
    curve->add_point(
        (iscore_segment->*CurveTraits<Y_T>::fun)(),
          scale_x(end.x()), end.y());
#endif
  }

  return curve;
}
}
}
