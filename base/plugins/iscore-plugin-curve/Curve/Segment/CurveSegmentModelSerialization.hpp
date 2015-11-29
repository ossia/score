#pragma once
class CurveSegmentModel;
class DataStream;
class DynamicCurveSegmentList;
class JSONObject;
class QObject;
struct CurveSegmentData;
template <typename VisitorType> class Visitor;

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
