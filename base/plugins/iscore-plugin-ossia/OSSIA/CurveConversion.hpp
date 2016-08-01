#pragma once

#include <Editor/Curve.h>
#include <Editor/CurveSegment/CurveSegmentLinear.h>
#include <Editor/CurveSegment/CurveSegmentPower.h>
#include <OSSIA/Curve/EasingSegment.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>


namespace iscore
{
namespace convert
{
template<typename Ease, typename Y>
struct CurveSegmentEase : public OSSIA::CurveSegment<Y>
{
        std::shared_ptr<OSSIA::CurveAbstract> parent;
        template<typename T>
        CurveSegmentEase(T other):
            parent{other}
        {
        }

        Y valueAt(double x, Y start, Y end) const override
        {
            return ossia::easing::ease<Y>{}(start, end, Ease{}(x));
        }

        typename OSSIA::CurveSegment<Y>::Type getType() const override
        { return OSSIA::CurveSegment<Y>::Type::POWER; }

        std::shared_ptr<OSSIA::CurveAbstract> getParent() const override
        { return parent; }
};

template<typename X, typename Y>
std::shared_ptr<OSSIA::CurveSegment<Y>> make_easing(
        std::shared_ptr<OSSIA::Curve<X, Y>> c, Curve::SegmentFactory::ConcreteFactoryKey k)
{
    using namespace Ossia::EasingCurve;
    if(k == Metadata<ConcreteFactoryKey_k, Segment_backIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_backIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_backOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_backOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_backInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_backInOut::easing_type, Y>>(c);

    if(k == Metadata<ConcreteFactoryKey_k, Segment_bounceIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_bounceIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_bounceOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_bounceOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_bounceInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_bounceInOut::easing_type, Y>>(c);

    if(k == Metadata<ConcreteFactoryKey_k, Segment_quadraticIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_quadraticIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_quadraticOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_quadraticOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_quadraticInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_quadraticInOut::easing_type, Y>>(c);

    if(k == Metadata<ConcreteFactoryKey_k, Segment_cubicIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_cubicIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_cubicOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_cubicOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_cubicInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_cubicInOut::easing_type, Y>>(c);

    if(k == Metadata<ConcreteFactoryKey_k, Segment_quarticIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_quarticIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_quarticOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_quarticOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_quarticInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_quarticInOut::easing_type, Y>>(c);

    if(k == Metadata<ConcreteFactoryKey_k, Segment_quinticIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_quinticIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_quinticOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_quinticOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_quinticInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_quinticInOut::easing_type, Y>>(c);

    if(k == Metadata<ConcreteFactoryKey_k, Segment_sineIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_sineIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_sineOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_sineOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_sineInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_sineInOut::easing_type, Y>>(c);

    if(k == Metadata<ConcreteFactoryKey_k, Segment_circularIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_circularIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_circularOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_circularOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_circularInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_circularInOut::easing_type, Y>>(c);

    if(k == Metadata<ConcreteFactoryKey_k, Segment_exponentialIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_exponentialIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_exponentialOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_exponentialOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_exponentialInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_exponentialInOut::easing_type, Y>>(c);

    if(k == Metadata<ConcreteFactoryKey_k, Segment_elasticIn>::get())
        return std::make_shared<CurveSegmentEase<Segment_elasticIn::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_elasticOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_elasticOut::easing_type, Y>>(c);
    if(k == Metadata<ConcreteFactoryKey_k, Segment_elasticInOut>::get())
        return std::make_shared<CurveSegmentEase<Segment_elasticInOut::easing_type, Y>>(c);
    return {};
}

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
        else
        {
            curve->addPoint(make_easing<X_T, Y_T>(curve, iscore_segment.type),
                            scale_x(iscore_segment.end.x()),
                            scale_y(iscore_segment.end.y()));

        }
    }
    return curve;
}

}
}
