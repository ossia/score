#pragma once

class QObject;
namespace Curve
{
class SegmentModel;
class SegmentList;
struct SegmentData;

SegmentModel* createCurveSegment(
        const SegmentList& csl,
        const SegmentData& dat,
        QObject* parent);
}
