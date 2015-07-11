#include "PowerCurveSegmentModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const PowerCurveSegmentModel& segmt)
{
    m_stream << segmt.gamma;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(PowerCurveSegmentModel& segmt)
{
    m_stream >> segmt.gamma;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const PowerCurveSegmentModel& segmt)
{
    m_obj["Power"] = segmt.gamma;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(PowerCurveSegmentModel& segmt)
{
    segmt.gamma = m_obj["Power"].toDouble();
}
