// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QDebug>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
namespace Curve
{
class PointArraySegment;
struct PointArraySegmentData;
}

template <>
void DataStreamReader::read(const Curve::PointArraySegment& segmt)
{
  SCORE_TODO;
}

template <>
void DataStreamWriter::write(Curve::PointArraySegment& segmt)
{
  SCORE_TODO;
}

template <>
void JSONObjectReader::read(const Curve::PointArraySegment& segmt)
{
  SCORE_TODO;
}

template <>
void JSONObjectWriter::write(Curve::PointArraySegment& segmt)
{
  SCORE_TODO;
}

template <>
void DataStreamReader::read(const Curve::PointArraySegmentData& segmt)
{
  SCORE_TODO;
}

template <>
void DataStreamWriter::write(Curve::PointArraySegmentData& segmt)
{
  SCORE_TODO;
}

template <>
void JSONObjectReader::read(const Curve::PointArraySegmentData& segmt)
{
  SCORE_TODO;
}

template <>
void JSONObjectWriter::write(Curve::PointArraySegmentData& segmt)
{
  SCORE_TODO;
}
