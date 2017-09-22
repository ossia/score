// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

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
void DataStreamWriter::write(Curve::LinearSegment& segmt)
{
}


template <>
void JSONObjectReader::read(
    const Curve::LinearSegment& segmt)
{
}


template <>
void JSONObjectWriter::write(Curve::LinearSegment& segmt)
{
}


template <>
void DataStreamReader::read(
    const Curve::LinearSegmentData& segmt)
{
}


template <>
void DataStreamWriter::write(Curve::LinearSegmentData& segmt)
{
}


template <>
void JSONObjectReader::read(
    const Curve::LinearSegmentData& segmt)
{
}

template <>
void JSONObjectWriter::write(Curve::LinearSegmentData& segmt)
{
}
