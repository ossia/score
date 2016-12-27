#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace Curve
{
class LinearSegment;
struct LinearSegmentData;
}

template <>
void DataStreamReader::read(
    const Curve::LinearSegment& segmt)
{
}


template <>
void DataStreamWriter::writeTo(Curve::LinearSegment& segmt)
{
}


template <>
void JSONObjectReader::read(
    const Curve::LinearSegment& segmt)
{
}


template <>
void JSONObjectWriter::writeTo(Curve::LinearSegment& segmt)
{
}


template <>
void DataStreamReader::read(
    const Curve::LinearSegmentData& segmt)
{
}


template <>
void DataStreamWriter::writeTo(Curve::LinearSegmentData& segmt)
{
}


template <>
void JSONObjectReader::read(
    const Curve::LinearSegmentData& segmt)
{
}

template <>
void JSONObjectWriter::writeTo(Curve::LinearSegmentData& segmt)
{
}
