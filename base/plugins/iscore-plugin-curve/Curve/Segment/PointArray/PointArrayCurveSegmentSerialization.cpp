#include "PointArrayCurveSegmentModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
template<>
void Visitor<Reader<DataStream>>::readFrom(const PointArrayCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(PointArrayCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const PointArrayCurveSegmentModel& segmt)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(PointArrayCurveSegmentModel& segmt)
{
}
