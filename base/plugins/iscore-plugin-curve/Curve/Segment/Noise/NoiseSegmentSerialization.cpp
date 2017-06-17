#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace Curve
{
class NoiseSegment;
struct NoiseSegmentData;
}

template <>
void DataStreamReader::read(
    const Curve::NoiseSegment& segmt)
{
}


template <>
void DataStreamWriter::write(Curve::NoiseSegment& segmt)
{
}


template <>
void JSONObjectReader::read(
    const Curve::NoiseSegment& segmt)
{
}


template <>
void JSONObjectWriter::write(Curve::NoiseSegment& segmt)
{
}


template <>
void DataStreamReader::read(
    const Curve::NoiseSegmentData& segmt)
{
}


template <>
void DataStreamWriter::write(Curve::NoiseSegmentData& segmt)
{
}


template <>
void JSONObjectReader::read(
    const Curve::NoiseSegmentData& segmt)
{
}

template <>
void JSONObjectWriter::write(Curve::NoiseSegmentData& segmt)
{
}
