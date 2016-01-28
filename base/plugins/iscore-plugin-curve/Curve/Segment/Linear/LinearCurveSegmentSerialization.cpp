#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace Curve
{
class LinearSegment;
struct LinearSegmentData;
}
template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Curve::LinearSegment& segmt)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Curve::LinearSegment& segmt)
{
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Curve::LinearSegment& segmt)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Curve::LinearSegment& segmt)
{
}

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Curve::LinearSegmentData& segmt)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Curve::LinearSegmentData& segmt)
{
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Curve::LinearSegmentData& segmt)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Curve::LinearSegmentData& segmt)
{
}
