#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>

#include "GammaCurveSegmentModel.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Curve::GammaSegment& segmt)
{
    m_stream << segmt.gamma;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Curve::GammaSegment& segmt)
{
    m_stream >> segmt.gamma;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Curve::GammaSegment& segmt)
{
    m_obj["Gamma"] = segmt.gamma;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Curve::GammaSegment& segmt)
{
    segmt.gamma = m_obj["Gamma"].toDouble();
}


template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Curve::GammaSegmentData& segmt)
{
    m_stream << segmt.gamma;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Curve::GammaSegmentData& segmt)
{
    m_stream >> segmt.gamma;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Curve::GammaSegmentData& segmt)
{
    m_obj["Power"] = segmt.gamma;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Curve::GammaSegmentData& segmt)
{
    segmt.gamma = m_obj["Power"].toDouble();
}
