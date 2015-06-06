#pragma once
class CurveSegmentModel;
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

CurveSegmentModel* createCurveSegment(
        Deserializer<DataStream>& deserializer,
        QObject* parent);

CurveSegmentModel* createCurveSegment(
        Deserializer<JSONObject>& deserializer,
        QObject* parent);
