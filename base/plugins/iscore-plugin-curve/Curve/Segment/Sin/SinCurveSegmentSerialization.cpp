#include "SinCurveSegmentModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const SinCurveSegmentModel& segmt)
{
    m_stream << segmt.freq << segmt.ampl;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(SinCurveSegmentModel& segmt)
{
    m_stream >> segmt.freq >> segmt.ampl;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const SinCurveSegmentModel& segmt)
{
    m_obj["Freq"] = segmt.freq;
    m_obj["Ampl"] = segmt.ampl;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(SinCurveSegmentModel& segmt)
{
    segmt.freq = m_obj["Freq"].toDouble();
    segmt.ampl = m_obj["Ampl"].toDouble();
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const SinCurveSegmentData& segmt)
{
    m_stream << segmt.freq << segmt.ampl;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(SinCurveSegmentData& segmt)
{
    m_stream >> segmt.freq >> segmt.ampl;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const SinCurveSegmentData& segmt)
{
    m_obj["Freq"] = segmt.freq;
    m_obj["Ampl"] = segmt.ampl;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(SinCurveSegmentData& segmt)
{
    segmt.freq = m_obj["Freq"].toDouble();
    segmt.ampl = m_obj["Ampl"].toDouble();
}
