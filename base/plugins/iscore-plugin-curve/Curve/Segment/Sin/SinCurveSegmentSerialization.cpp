#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>

#include "SinCurveSegmentModel.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Curve::SinSegment& segmt)
{
    m_stream << segmt.freq << segmt.ampl;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Curve::SinSegment& segmt)
{
    m_stream >> segmt.freq >> segmt.ampl;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Curve::SinSegment& segmt)
{
    m_obj["Freq"] = segmt.freq;
    m_obj["Ampl"] = segmt.ampl;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Curve::SinSegment& segmt)
{
    segmt.freq = m_obj["Freq"].toDouble();
    segmt.ampl = m_obj["Ampl"].toDouble();
}


template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Curve::SinSegmentData& segmt)
{
    m_stream << segmt.freq << segmt.ampl;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Curve::SinSegmentData& segmt)
{
    m_stream >> segmt.freq >> segmt.ampl;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Curve::SinSegmentData& segmt)
{
    m_obj["Freq"] = segmt.freq;
    m_obj["Ampl"] = segmt.ampl;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Curve::SinSegmentData& segmt)
{
    segmt.freq = m_obj["Freq"].toDouble();
    segmt.ampl = m_obj["Ampl"].toDouble();
}
