#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>

#include "PowerCurveSegmentModel.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(
        const Curve::PowerSegment& segmt)
{
    m_stream << segmt.gamma;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Curve::PowerSegment& segmt)
{
    m_stream >> segmt.gamma;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Curve::PowerSegment& segmt)
{
    m_obj["Power"] = segmt.gamma;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Curve::PowerSegment& segmt)
{
    segmt.gamma = m_obj["Power"].toDouble();
}


template<>
void Visitor<Reader<DataStream>>::readFrom_impl(
        const Curve::PowerSegmentData& segmt)
{
    m_stream << segmt.gamma;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Curve::PowerSegmentData& segmt)
{
    m_stream >> segmt.gamma;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Curve::PowerSegmentData& segmt)
{
    m_obj["Power"] = segmt.gamma;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Curve::PowerSegmentData& segmt)
{
    segmt.gamma = m_obj["Power"].toDouble();
}
