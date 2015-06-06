#pragma once
#include "CurveSegmentFactory.hpp"

// TODO candidate to template
class LinearCurveSegmentFactory : public CurveSegmentFactory
{
    public:
        QString name() const;

        CurveSegmentModel *make(
                const id_type<CurveSegmentModel>& id,
                QObject* parent) override;

        CurveSegmentModel *load(
                const VisitorVariant& vis,
                QObject* parent) override;
};


