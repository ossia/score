#pragma once
#include "CurveSegmentFactory.hpp"

// Template this
class AutomationControl;
class CurveSegmentList
{
    public:
        CurveSegmentList() = default;
        CurveSegmentList(const CurveSegmentList&) = delete;
        CurveSegmentFactory* get(const QString& name);
        void registerFactory(CurveSegmentFactory* fact);

    private:
        QVector<CurveSegmentFactory*> factories;
};


class SingletonCurveSegmentList
{
    public:
        SingletonCurveSegmentList() = delete;
        static CurveSegmentList& instance();
};
