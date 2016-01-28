#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/CurveSegmentModelSerialization.hpp>


#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <sys/types.h>

#include "CurveModel.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;

template<>
ISCORE_PLUGIN_CURVE_EXPORT void Visitor<Reader<DataStream>>::readFrom(
        const Curve::Model& curve)
{
    readFrom(static_cast<const IdentifiedObject<Curve::Model>&>(curve));

    const auto& segments = curve.segments();

    m_stream << (int32_t)segments.size();
    for(const auto& seg : segments)
    {
        readFrom(seg);
    }
    insertDelimiter();
}

template<>
ISCORE_PLUGIN_CURVE_EXPORT void Visitor<Writer<DataStream>>::writeTo(
        Curve::Model& curve)
{
    int32_t size;
    m_stream >> size;

    auto& csl = context.components.factory<Curve::SegmentList>();
    for(; size --> 0;)
    {
        curve.addSegment(deserialize_interface(csl, *this, &curve));
    }

    curve.changed();
    checkDelimiter();
}

template<>
ISCORE_PLUGIN_CURVE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(
        const Curve::Model& curve)
{
    readFrom(static_cast<const IdentifiedObject<Curve::Model>&>(curve));

    m_obj["Segments"] = toJsonArray(curve.segments());
}

template<>
ISCORE_PLUGIN_CURVE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(
        Curve::Model& curve)
{
    auto& csl = context.components.factory<Curve::SegmentList>();
    for(const auto& segment : m_obj["Segments"].toArray())
    {
        Deserializer<JSONObject> segment_deser{segment.toObject()};
        curve.addSegment(deserialize_interface(csl, segment_deser, &curve));
    }

    curve.changed();
}


