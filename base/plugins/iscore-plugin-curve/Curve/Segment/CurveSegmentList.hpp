#pragma once
#include "CurveSegmentFactory.hpp"

// TODO Template this
class CurveSegmentList
{
    public:
        CurveSegmentList() = default;
        CurveSegmentList(const CurveSegmentList&) = delete;
        CurveSegmentFactory* get(const QString& name) const;
        void registerFactory(CurveSegmentFactory* fact);

        QStringList nameList() const;

    private:
        QVector<CurveSegmentFactory*> factories;
};


class SingletonCurveSegmentList
{
    public:
        SingletonCurveSegmentList() = delete;
        static CurveSegmentList& instance();
};
