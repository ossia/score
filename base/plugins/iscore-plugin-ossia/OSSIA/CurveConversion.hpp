#pragma once

#include <Editor/Curve.h>
#include <Editor/CurveSegment/CurveSegmentLinear.h>
#include <Editor/CurveSegment/CurveSegmentPower.h>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>


namespace iscore
{
namespace convert
{

template<typename X_T, typename Y_T, typename XScaleFun, typename YScaleFun, typename Segments>
std::shared_ptr<OSSIA::CurveAbstract> curve(
        XScaleFun scale_x,
        YScaleFun scale_y,
        const Segments& segments)
{
    auto curve = OSSIA::Curve<X_T, Y_T>::create();
    if(segments[0].start.x() == 0.)
    {
        curve->setInitialPointAbscissa(scale_x(segments[0].start.x()));
        curve->setInitialPointOrdinate(scale_y(segments[0].start.y()));
    }

    for(const auto& iscore_segment : segments)
    {
        if(iscore_segment.type == Metadata<ConcreteFactoryKey_k, Curve::LinearSegment>::get())
        {
            curve->addPoint(
                        OSSIA::CurveSegmentLinear<Y_T>::create(curve),
                        scale_x(iscore_segment.end.x()),
                        scale_y(iscore_segment.end.y()));
        }
        else if(iscore_segment.type == Metadata<ConcreteFactoryKey_k, Curve::PowerSegment>::get())
        {
            auto val = iscore_segment.specificSegmentData.template value<Curve::PowerSegmentData>();

            if(val.gamma == Curve::PowerSegmentData::linearGamma)
            {
                curve->addPoint(
                            OSSIA::CurveSegmentLinear<Y_T>::create(curve),
                            scale_x(iscore_segment.end.x()),
                            scale_y(iscore_segment.end.y()));
            }
            else
            {
                auto powSegment = OSSIA::CurveSegmentPower<Y_T>::create(curve);
                powSegment->setPower(Curve::PowerSegmentData::linearGamma + 1 - val.gamma);

                curve->addPoint(
                            powSegment,
                            scale_x(iscore_segment.end.x()),
                            scale_y(iscore_segment.end.y()));
            }
        }
    }
    return curve;
}

}
}
