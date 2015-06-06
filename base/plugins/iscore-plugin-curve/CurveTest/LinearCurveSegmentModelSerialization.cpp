#include "LinearCurveSegmentModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
template<>
void Visitor<Reader<DataStream>>::readFrom(const LinearCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(LinearCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const LinearCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(LinearCurveSegmentModel& segmt)
{
}



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
