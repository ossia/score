#include "GammaCurveSegmentModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const GammaCurveSegmentModel& segmt)
{
    m_stream << segmt.gamma;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(GammaCurveSegmentModel& segmt)
{
    m_stream >> segmt.gamma;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const GammaCurveSegmentModel& segmt)
{
    m_obj["Gamma"] = segmt.gamma;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(GammaCurveSegmentModel& segmt)
{
    segmt.gamma = m_obj["Gamma"].toDouble();
}
