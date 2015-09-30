#pragma once
#include <Curve/Segment/CurveSegmentData.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

CurveSegmentModel* createCurveSegment(
        Deserializer<DataStream>& deserializer,
        QObject* parent);

CurveSegmentModel* createCurveSegment(
        Deserializer<JSONObject>& deserializer,
        QObject* parent);

CurveSegmentModel* createCurveSegment(
        const CurveSegmentData& dat,
        QObject* parent);
