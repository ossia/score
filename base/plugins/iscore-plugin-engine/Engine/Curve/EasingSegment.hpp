#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <iscore_plugin_engine_export.h>


namespace Engine
{
namespace EasingCurve
{
struct EasingData {};

template<typename T>
class EasingSegment;
}
}
template<typename T>
struct TSerializer<DataStream, void, Engine::EasingCurve::EasingSegment<T>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const Engine::EasingCurve::EasingSegment<T>& obj);
        static void writeTo(
                DataStream::Deserializer& s,
                Engine::EasingCurve::EasingSegment<T>& obj);
};

template<typename T>
struct TSerializer<JSONObject, Engine::EasingCurve::EasingSegment<T>>
{
        static void readFrom(
                JSONObject::Serializer& s,
                const Engine::EasingCurve::EasingSegment<T>& obj);
        static void writeTo(
                JSONObject::Deserializer& s,
                Engine::EasingCurve::EasingSegment<T>& obj);
};


namespace Engine
{
namespace EasingCurve
{
template<typename Easing_T>
class EasingSegment final :
        public ::Curve::SegmentModel
{
    public:
        key_type concreteFactoryKey() const final override
        {
            return Metadata<ConcreteFactoryKey_k, EasingSegment>::get();
        }

        EasingSegment* clone(
            const id_type& newId,
            QObject* newParent) const final override\
        {
           return new EasingSegment{*this, newId, newParent};
        }

        void serialize_impl(const VisitorVariant& vis) const override
        {

        }

        using data_type = EasingData;
        template<typename... Args>
        EasingSegment(Args&&... args):
            Curve::SegmentModel{std::forward<Args>(args)...}
        {

        }

        EasingSegment(
                const EasingSegment& other,
                const Curve::SegmentModel::id_type& id,
                QObject* parent):
            Curve::SegmentModel{other.start(), other.end(), id, parent}
        {

        }

        template<typename Impl>
        EasingSegment(Deserializer<Impl>& vis, QObject* parent) :
            Curve::SegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        void on_startChanged() override
        {
            emit dataChanged();
        }
        void on_endChanged() override
        {
            emit dataChanged();
        }

        void updateData(int numInterp) const override
        {
            if(std::size_t(2 * numInterp + 1) != m_data.size())
                m_valid = false;
            if(!m_valid)
            {
                numInterp *= 2;
                m_data.resize(numInterp + 1);

                double start_x = start().x();
                double end_x = end().x();

                double start_y = start().y();
                double end_y = end().y();
                for(int j = 0; j <= numInterp; j++)
                {
                    QPointF& pt = m_data[j];
                    pt.setX(start_x + (double(j) / numInterp) * (end_x - start_x));
                    pt.setY(start_y + (end_y - start_y) * Easing_T{}(double(j) / numInterp));
                }
            }
        }

        double valueAt(double x) const override
        {
            return start().y() + (end().y() - start().y()) * (Easing_T{}(x) - start().x()) / (end().x() - start().x());
        }

        optional<double> verticalParameter() const override { return {}; }
        optional<double> horizontalParameter() const override { return {}; }
        void setVerticalParameter(double p) override { }
        void setHorizontalParameter(double p) override { }

        QVariant toSegmentSpecificData() const override
        {
            return QVariant::fromValue(EasingData{});
        }

        template<typename Y>
        std::function<Y(double, Y, Y)> makeFunction() const
        {
            return [=] (double ratio, Y start, Y end) {
                return start + Easing_T{}(ratio) * (end - start);
            };
        }
        std::function<float(double, float, float)> makeFloatFunction() const override
        { return makeFunction<float>(); }
        std::function<int(double, int, int)> makeIntFunction() const override
        { return makeFunction<int>(); }
        std::function<bool(double, bool, bool)> makeBoolFunction() const override
        { return makeFunction<bool>(); }
};
using Segment_backIn = EasingSegment<ossia::easing::backIn<double>>;
using Segment_backOut = EasingSegment<ossia::easing::backOut<double>>;
using Segment_backInOut = EasingSegment<ossia::easing::backInOut<double>>;
using Segment_bounceIn = EasingSegment<ossia::easing::bounceIn<double>>;
using Segment_bounceOut = EasingSegment<ossia::easing::bounceOut<double>>;
using Segment_bounceInOut = EasingSegment<ossia::easing::bounceInOut<double>>;
using Segment_quadraticIn = EasingSegment<ossia::easing::quadraticIn<double>>;
using Segment_quadraticOut = EasingSegment<ossia::easing::quadraticOut<double>>;
using Segment_quadraticInOut = EasingSegment<ossia::easing::quadraticInOut<double>>;
using Segment_cubicIn = EasingSegment<ossia::easing::cubicIn<double>>;
using Segment_cubicOut = EasingSegment<ossia::easing::cubicOut<double>>;
using Segment_cubicInOut = EasingSegment<ossia::easing::cubicInOut<double>>;
using Segment_quarticIn = EasingSegment<ossia::easing::quarticIn<double>>;
using Segment_quarticOut = EasingSegment<ossia::easing::quarticOut<double>>;
using Segment_quarticInOut = EasingSegment<ossia::easing::quarticInOut<double>>;
using Segment_quinticIn = EasingSegment<ossia::easing::quinticIn<double>>;
using Segment_quinticOut = EasingSegment<ossia::easing::quinticOut<double>>;
using Segment_quinticInOut = EasingSegment<ossia::easing::quinticInOut<double>>;
using Segment_sineIn = EasingSegment<ossia::easing::sineIn<double>>;
using Segment_sineOut = EasingSegment<ossia::easing::sineOut<double>>;
using Segment_sineInOut = EasingSegment<ossia::easing::sineInOut<double>>;
using Segment_circularIn = EasingSegment<ossia::easing::circularIn<double>>;
using Segment_circularOut = EasingSegment<ossia::easing::circularOut<double>>;
using Segment_circularInOut = EasingSegment<ossia::easing::circularInOut<double>>;
using Segment_exponentialIn = EasingSegment<ossia::easing::exponentialIn<double>>;
using Segment_exponentialOut = EasingSegment<ossia::easing::exponentialOut<double>>;
using Segment_exponentialInOut = EasingSegment<ossia::easing::exponentialInOut<double>>;
using Segment_elasticIn = EasingSegment<ossia::easing::elasticIn<double>>;
using Segment_elasticOut = EasingSegment<ossia::easing::elasticOut<double>>;
using Segment_elasticInOut = EasingSegment<ossia::easing::elasticInOut<double>>;


}
}


template<>
inline void Visitor<Reader<DataStream>>::readFrom_impl(
        const Engine::EasingCurve::EasingData& segmt)
{
}

template<>
inline void Visitor<Writer<DataStream>>::writeTo(
        Engine::EasingCurve::EasingData& segmt)
{
}

template<>
inline void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Engine::EasingCurve::EasingData& segmt)
{
}

template<>
inline void Visitor<Writer<JSONObject>>::writeTo(
        Engine::EasingCurve::EasingData& segmt)
{
}


template<typename T>
void TSerializer<DataStream, void, Engine::EasingCurve::EasingSegment<T>>::readFrom(
        DataStream::Serializer& s,
        const Engine::EasingCurve::EasingSegment<T>& obj)
{

}
template<typename T>
void TSerializer<DataStream, void, Engine::EasingCurve::EasingSegment<T>>::writeTo(
        DataStream::Deserializer& s,
        Engine::EasingCurve::EasingSegment<T>& obj)
{

}

template<typename T>
void TSerializer<JSONObject, Engine::EasingCurve::EasingSegment<T>>::readFrom(
        JSONObject::Serializer& s,
        const Engine::EasingCurve::EasingSegment<T>& obj)
{

}
template<typename T>
void TSerializer<JSONObject, Engine::EasingCurve::EasingSegment<T>>::writeTo(
        JSONObject::Deserializer& s,
        Engine::EasingCurve::EasingSegment<T>& obj)
{

}



Q_DECLARE_METATYPE(Engine::EasingCurve::EasingData)

// cat easings | xargs -L1 bash -c 'echo $(uuidgen)' | paste - easings | sed 's/\t/ /' > easings2
// cat easings2 | awk '{ print "CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_" $2 ">, \"" $1 "\", \"" $2 "\", \"" $2 "\")";} '

CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_backIn, "fb5cb6c1-47fd-497c-9d69-7a87adbaf3b3", "backIn", "backIn")

CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_backOut, "0edbd8f5-67c2-41f2-ae80-f014e5c24aa6", "backOut", "backOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_backInOut, "3825c351-698d-4930-9862-28c5f7f51c61", "backInOut", "backInOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_bounceIn, "51fafa98-aa8e-48f0-adae-c21c3eeb63ca", "bounceIn", "bounceIn")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_bounceOut, "75ce6961-22b3-4a9e-b989-3131098bd092", "bounceOut", "bounceOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_bounceInOut, "30d5c3dc-5a8c-44a8-95b4-67ca3ff5088b", "bounceInOut", "bounceInOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_quadraticIn, "a2f38b24-f2c9-42d7-bb5e-a51a821d2ffd", "quadraticIn", "quadraticIn")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_quadraticOut, "9717560b-fae1-4035-afbf-031f3581d132", "quadraticOut", "quadraticOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_quadraticInOut, "fc333d55-064a-4af8-b4a8-23fe16e80ecc", "quadraticInOut", "quadraticInOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_cubicIn, "b6bddf8b-b2cb-46d6-8d90-31b3029317e8", "cubicIn", "cubicIn")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_cubicOut, "ff9a7726-2d8f-43f2-a95c-e395fdbd5aa9", "cubicOut", "cubicOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_cubicInOut, "21556d2a-ac8a-4acf-bf42-5eca9795047c", "cubicInOut", "cubicInOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_quarticIn, "72678775-535f-438e-b4bb-1077af3fab99", "quarticIn", "quarticIn")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_quarticOut, "bd6f4867-2c30-4267-aa97-0ed767883d22", "quarticOut", "quarticOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_quarticInOut, "93cac01a-dedc-4cb4-ad2d-d81b31e6187c", "quarticInOut", "quarticInOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_quinticIn, "cd08eabe-51a4-429e-8740-c097c1b34b83", "quinticIn", "quinticIn")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_quinticOut, "cb6e340b-faee-440c-8ef5-05ec6f7f4791", "quinticOut", "quinticOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_quinticInOut, "b2cce6a7-c651-429f-ae46-d322991e92d4", "quinticInOut", "quinticInOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_sineIn, "e8a44b49-4d91-4066-8e25-7669d8927792", "sineIn", "sineIn")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_sineOut, "e5d2dea4-061e-4139-8da2-1d21b0414273", "sineOut", "sineOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_sineInOut, "61bd1bb4-353b-435f-8b4c-ed61ed43bcf9", "sineInOut", "sineInOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_circularIn, "45b06858-2e16-4cdc-82cc-70f8545bab03", "circularIn", "circularIn")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_circularOut, "32a67b2a-61f5-49cd-b1c4-37419350fca8", "circularOut", "circularOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_circularInOut, "b70a58f2-2fff-4fda-b1fa-cad6c7c11b88", "circularInOut", "circularInOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_exponentialIn, "8db88491-bed5-4a0d-bcb6-45811c5a7722", "exponentialIn", "exponentialIn")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_exponentialOut, "40f17c1a-9611-4468-b114-c6338fb0fbb7", "exponentialOut", "exponentialOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_exponentialInOut, "46224c2f-f60d-4f60-93b8-a0ebe6931d00", "exponentialInOut", "exponentialInOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_elasticIn, "6e301164-e079-466b-a518-12fe89048283", "elasticIn", "elasticIn")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_elasticOut, "1f1fddd4-7a23-4c15-a3ec-6e88e9787a97", "elasticOut", "elasticOut")
CURVE_SEGMENT_METADATA(ISCORE_PLUGIN_ENGINE_EXPORT, Engine::EasingCurve::Segment_elasticInOut, "8bad1486-b616-4ebe-aa5e-844162545f8b", "elasticInOut", "elasticInOut")

namespace Engine { namespace EasingCurve {
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_backIn, Segment_backIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_backOut, Segment_backOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_backInOut, Segment_backInOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_bounceIn, Segment_bounceIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_bounceOut, Segment_bounceOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_bounceInOut, Segment_bounceInOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_quadraticIn, Segment_quadraticIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_quadraticOut, Segment_quadraticOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_quadraticInOut, Segment_quadraticInOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_cubicIn, Segment_cubicIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_cubicOut, Segment_cubicOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_cubicInOut, Segment_cubicInOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_quarticIn, Segment_quarticIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_quarticOut, Segment_quarticOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_quarticInOut, Segment_quarticInOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_quinticIn, Segment_quinticIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_quinticOut, Segment_quinticOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_quinticInOut, Segment_quinticInOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_sineIn, Segment_sineIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_sineOut, Segment_sineOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_sineInOut, Segment_sineInOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_circularIn, Segment_circularIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_circularOut, Segment_circularOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_circularInOut, Segment_circularInOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_exponentialIn, Segment_exponentialIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_exponentialOut, Segment_exponentialOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_exponentialInOut, Segment_exponentialInOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_elasticIn, Segment_elasticIn)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_elasticOut, Segment_elasticOut)
DEFINE_CURVE_SEGMENT_FACTORY(SegmentFactory_elasticInOut, Segment_elasticInOut)
    } }
