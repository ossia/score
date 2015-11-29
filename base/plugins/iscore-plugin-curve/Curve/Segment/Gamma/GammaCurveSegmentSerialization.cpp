#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include "GammaCurveSegmentModel.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

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


template<>
void Visitor<Reader<DataStream>>::readFrom(const GammaCurveSegmentData& segmt)
{
    m_stream << segmt.gamma;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(GammaCurveSegmentData& segmt)
{
    m_stream >> segmt.gamma;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const GammaCurveSegmentData& segmt)
{
    m_obj["Power"] = segmt.gamma;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(GammaCurveSegmentData& segmt)
{
    segmt.gamma = m_obj["Power"].toDouble();
}
