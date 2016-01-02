#pragma once
class DataStream;
class JSONObject;
class QObject;
template <typename VisitorType> class Visitor;

namespace Curve
{
class SegmentModel;
class SegmentList;
struct SegmentData;
SegmentModel* createCurveSegment(
        const SegmentList& csl,
        Deserializer<DataStream>& deserializer,
        QObject* parent);

SegmentModel* createCurveSegment(
        const SegmentList& csl,
        Deserializer<JSONObject>& deserializer,
        QObject* parent);

SegmentModel* createCurveSegment(
        const SegmentList& csl,
        const SegmentData& dat,
        QObject* parent);
}
