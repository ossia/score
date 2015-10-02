#include "LinearCurveSegmentModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
template<>
void Visitor<Reader<DataStream>>::readFrom(const LinearCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(LinearCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const LinearCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(LinearCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Reader<DataStream>>::readFrom(const LinearCurveSegmentData& segmt)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(LinearCurveSegmentData& segmt)
{
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const LinearCurveSegmentData& segmt)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(LinearCurveSegmentData& segmt)
{
}
