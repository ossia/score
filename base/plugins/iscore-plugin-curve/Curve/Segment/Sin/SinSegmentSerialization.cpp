#include <QJsonObject>
#include <QJsonValue>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "SinSegment.hpp"


template <>
void DataStreamReader::read(const Curve::SinSegment& segmt)
{
  m_stream << segmt.freq << segmt.ampl;
}


template <>
void DataStreamWriter::writeTo(Curve::SinSegment& segmt)
{
  m_stream >> segmt.freq >> segmt.ampl;
}


template <>
void JSONObjectReader::readFromConcrete(const Curve::SinSegment& segmt)
{
  obj["Freq"] = segmt.freq;
  obj["Ampl"] = segmt.ampl;
}


template <>
void JSONObjectWriter::writeTo(Curve::SinSegment& segmt)
{
  segmt.freq = obj["Freq"].toDouble();
  segmt.ampl = obj["Ampl"].toDouble();
}


template <>
void DataStreamReader::read(const Curve::SinSegmentData& segmt)
{
  m_stream << segmt.freq << segmt.ampl;
}


template <>
void DataStreamWriter::writeTo(Curve::SinSegmentData& segmt)
{
  m_stream >> segmt.freq >> segmt.ampl;
}


template <>
void JSONObjectReader::readFrom(const Curve::SinSegmentData& segmt)
{
  obj["Freq"] = segmt.freq;
  obj["Ampl"] = segmt.ampl;
}


template <>
void JSONObjectWriter::writeTo(Curve::SinSegmentData& segmt)
{
  segmt.freq = obj["Freq"].toDouble();
  segmt.ampl = obj["Ampl"].toDouble();
}
