#include <QDebug>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
namespace Curve
{
class PointArraySegment;
struct PointArraySegmentData;
}
template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
void Visitor<Reader<DataStream>>::read(
    const Curve::PointArraySegment& segmt)
{
  ISCORE_TODO;
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Curve::PointArraySegment& segmt)
{
  ISCORE_TODO;
}

template <>
void Visitor<Reader<JSONObject>>::readFromConcrete(
    const Curve::PointArraySegment& segmt)
{
  ISCORE_TODO;
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Curve::PointArraySegment& segmt)
{
  ISCORE_TODO;
}

template <>
void Visitor<Reader<DataStream>>::read(
    const Curve::PointArraySegmentData& segmt)
{
  ISCORE_TODO;
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Curve::PointArraySegmentData& segmt)
{
  ISCORE_TODO;
}

template <>
void Visitor<Reader<JSONObject>>::readFromConcrete(
    const Curve::PointArraySegmentData& segmt)
{
  ISCORE_TODO;
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Curve::PointArraySegmentData& segmt)
{
  ISCORE_TODO;
}
