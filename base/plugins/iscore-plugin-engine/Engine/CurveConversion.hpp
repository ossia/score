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
    curve->setInitialPointAbscissa(scale_x(start.x()));
    curve->setInitialPointOrdinate(scale_y(start.y()));
  }

  for (auto iscore_segment : segments)
  {
    auto end = iscore_segment->end();
#if defined(_MSC_VER)
    if (std::is_same<Y_T, int>::value)
    {
      curve->addPoint(
          iscore_segment->makeIntFunction(),
          scale_x(end.x()),
          scale_y(end.y()));
    }
    else if (std::is_same<Y_T, float>::value)
    {
      curve->addPoint(
          iscore_segment->makeFloatFunction(),
          scale_x(end.x()),
          scale_y(end.y()));
    }
    else if (std::is_same<Y_T, bool>::value)
    {
      curve->addPoint(
          iscore_segment->makeBoolFunction(),
          scale_x(end.x()),
          scale_y(end.y()));
    }
#else
    curve->addPoint(
        (iscore_segment->*CurveTraits<Y_T>::fun)(),
        scale_x(end.x()),
        scale_y(end.y()));
#endif
  }

  if (tween)
  {
    curve->setInitialPointOrdinateDestination(*tween);
  }

  return curve;
}
}
}
