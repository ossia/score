#pragma once
#include <ossia/editor/curve/curve.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>


namespace iscore
{
namespace convert
{

template<typename Y_T>
struct CurveTraits;

template<>
struct CurveTraits<int>
{
        static const constexpr auto fun = &Curve::SegmentModel::makeIntFunction;
};

template<>
struct CurveTraits<float>
{
        static const constexpr auto fun = &Curve::SegmentModel::makeFloatFunction;
};

template<>
struct CurveTraits<bool>
{
        static const constexpr auto fun = &Curve::SegmentModel::makeBoolFunction;
};


template<typename X_T, typename Y_T,
         typename XScaleFun, typename YScaleFun,
         typename Segments>
std::shared_ptr<OSSIA::CurveAbstract> curve(
        XScaleFun scale_x,
        YScaleFun scale_y,
        const Segments& segments,
        const optional<OSSIA::Destination>& tween)
{
    auto curve = OSSIA::Curve<X_T, Y_T>::create();

    auto start = segments[0]->start();
    if(start.x() == 0.)
    {
        curve->setInitialPointAbscissa(scale_x(start.x()));
        curve->setInitialPointOrdinate(scale_y(start.y()));
    }

    for(auto iscore_segment : segments)
    {
        auto end = iscore_segment->end();
        curve->addPoint(
                    (iscore_segment->*CurveTraits<Y_T>::fun)(),
                    scale_x(end.x()),
                    scale_y(end.y()));
    }

    if(tween)
    {
        curve->setInitialPointOrdinateDestination(*tween);
    }

    return curve;
}

}
}
