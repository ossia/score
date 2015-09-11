#include "CurveModel.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "Curve/Segment/CurveSegmentModelSerialization.hpp"
#include "Curve/Point/CurvePointModel.hpp"

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

    QVector<CurveSegmentModel*> v;
    for(; size --> 0;)
    {
        curve.addSegment(createCurveSegment(*this, &curve));
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
    for(const auto& segment : m_obj["Segments"].toArray())
    {
        Deserializer<JSONObject> segment_deser{segment.toObject()};
        curve.addSegment(createCurveSegment(segment_deser, &curve));
    }

    curve.changed();
}


