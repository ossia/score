#pragma once
#include "CurveSegmentFactory.hpp"

// Template this
class CurveSegmentList
{
    public:
        CurveSegmentFactory* get(const QString& name);

        void registration(CurveSegmentFactory* fact);

        static CurveSegmentList* instance();

    private:
        CurveSegmentList() = default;
        QVector<CurveSegmentFactory*> factories;
};
