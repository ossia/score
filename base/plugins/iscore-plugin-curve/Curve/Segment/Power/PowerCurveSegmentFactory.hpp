#pragma once
#include "Curve/Segment/CurveSegmentFactory.hpp"

class PowerCurveSegmentFactory : public CurveSegmentFactory
{
    public:
        QString name() const override;

        CurveSegmentModel *make(
                const Id<CurveSegmentModel>& id,
                QObject* parent) override;

        CurveSegmentModel *load(
                const VisitorVariant& vis,
                QObject* parent) override;
};
