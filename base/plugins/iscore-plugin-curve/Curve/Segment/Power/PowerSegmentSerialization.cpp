#include <QJsonObject>
#include <QJsonValue>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "PowerSegment.hpp"



template <>
void DataStreamReader::read(
    const Curve::PowerSegment& segmt)
{
  m_stream << segmt.gamma;
}


template <>
void DataStreamWriter::writeTo(Curve::PowerSegment& segmt)
{
  m_stream >> segmt.gamma;
}


template <>
void JSONObjectReader::read(
    const Curve::PowerSegment& segmt)
{
  obj["Power"] = segmt.gamma;
}


template <>
void JSONObjectWriter::writeTo(Curve::PowerSegment& segmt)
{
  segmt.gamma = obj["Power"].toDouble();
}


template <>
void DataStreamReader::read(
    const Curve::PowerSegmentData& segmt)
{
  m_stream << segmt.gamma;
}


template <>
void DataStreamWriter::writeTo(Curve::PowerSegmentData& segmt)
{
  m_stream >> segmt.gamma;
}


template <>
void JSONObjectReader::read(
    const Curve::PowerSegmentData& segmt)
{
  obj["Power"] = segmt.gamma;
}


template <>
void JSONObjectWriter::writeTo(Curve::PowerSegmentData& segmt)
{
  segmt.gamma = obj["Power"].toDouble();
}
