// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

namespace Curve
{
class NoiseSegment;
struct NoiseSegmentData;
}

template <>
void DataStreamReader::read(const Curve::NoiseSegment& segmt)
{
}

template <>
void DataStreamWriter::write(Curve::NoiseSegment& segmt)
{
}

template <>
void JSONObjectReader::read(const Curve::NoiseSegment& segmt)
{
}

template <>
void JSONObjectWriter::write(Curve::NoiseSegment& segmt)
{
}

template <>
void DataStreamReader::read(const Curve::NoiseSegmentData& segmt)
{
}

template <>
void DataStreamWriter::write(Curve::NoiseSegmentData& segmt)
{
}

template <>
void JSONObjectReader::read(const Curve::NoiseSegmentData& segmt)
{
}

template <>
void JSONObjectWriter::write(Curve::NoiseSegmentData& segmt)
{
}
