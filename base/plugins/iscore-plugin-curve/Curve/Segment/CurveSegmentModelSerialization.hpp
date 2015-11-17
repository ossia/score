#pragma once
#include <Curve/Segment/CurveSegmentData.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class DynamicCurveSegmentList;
CurveSegmentModel* createCurveSegment(
        const DynamicCurveSegmentList& csl,
        Deserializer<DataStream>& deserializer,
        QObject* parent);

CurveSegmentModel* createCurveSegment(
        const DynamicCurveSegmentList& csl,
        Deserializer<JSONObject>& deserializer,
        QObject* parent);

CurveSegmentModel* createCurveSegment(
        const DynamicCurveSegmentList& csl,
        const CurveSegmentData& dat,
        QObject* parent);
