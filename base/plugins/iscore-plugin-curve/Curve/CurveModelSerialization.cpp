#include "CurveModel.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <Curve/Segment/CurveSegmentModelSerialization.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const CurveModel& curve)
{
    readFrom(static_cast<const IdentifiedObject<CurveModel>&>(curve));

    const auto& segments = curve.segments();

    m_stream << (int32_t)segments.size();
    for(const auto& seg : segments)
    {
        readFrom(seg);
    }
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(CurveModel& curve)
{
    int32_t size;
    m_stream >> size;

    auto& csl = context.components.factory<DynamicCurveSegmentList>();
    QVector<CurveSegmentModel*> v;
    for(; size --> 0;)
    {
        curve.addSegment(createCurveSegment(csl, *this, &curve));
    }

    curve.changed();
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const CurveModel& curve)
{
    readFrom(static_cast<const IdentifiedObject<CurveModel>&>(curve));

    m_obj["Segments"] = toJsonArray(curve.segments());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(CurveModel& curve)
{
    auto& csl = context.components.factory<DynamicCurveSegmentList>();
    for(const auto& segment : m_obj["Segments"].toArray())
    {
        Deserializer<JSONObject> segment_deser{segment.toObject()};
        curve.addSegment(createCurveSegment(csl, segment_deser, &curve));
    }

    curve.changed();
}


