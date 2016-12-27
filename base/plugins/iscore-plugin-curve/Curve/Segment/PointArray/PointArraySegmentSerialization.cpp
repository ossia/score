#include <QDebug>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
namespace Curve
{
class PointArraySegment;
struct PointArraySegmentData;
}

template <>
void DataStreamReader::read(
    const Curve::PointArraySegment& segmt)
{
  ISCORE_TODO;
}


template <>
void DataStreamWriter::writeTo(Curve::PointArraySegment& segmt)
{
  ISCORE_TODO;
}


template <>
void JSONObjectReader::readFromConcrete(
    const Curve::PointArraySegment& segmt)
{
  ISCORE_TODO;
}


template <>
void JSONObjectWriter::writeTo(Curve::PointArraySegment& segmt)
{
  ISCORE_TODO;
}


template <>
void DataStreamReader::read(
    const Curve::PointArraySegmentData& segmt)
{
  ISCORE_TODO;
}


template <>
void DataStreamWriter::writeTo(Curve::PointArraySegmentData& segmt)
{
  ISCORE_TODO;
}


template <>
void JSONObjectReader::readFromConcrete(
    const Curve::PointArraySegmentData& segmt)
{
  ISCORE_TODO;
}


template <>
void JSONObjectWriter::writeTo(Curve::PointArraySegmentData& segmt)
{
  ISCORE_TODO;
}
