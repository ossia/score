#pragma once
#include <Curve/StateMachine/CurvePoint.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class CurveSegmentModel;

// An object wrapper useful for saving / loading
struct CurveSegmentData
{
        Id<CurveSegmentModel> id;

        CurvePoint start, end;
        Id<CurveSegmentModel> previous, following;

        QString type;
        QVariant specificSegmentData;
};

Q_DECLARE_METATYPE(CurveSegmentData)
