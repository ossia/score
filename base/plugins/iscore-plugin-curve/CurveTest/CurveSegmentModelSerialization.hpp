#pragma once
class CurveSegmentModel;
#include <iscore/serialization/DataStreamVisitor.hpp>

CurveSegmentModel* createCurveSegment(
        Deserializer<DataStream>& deserializer,
        QObject* parent);
