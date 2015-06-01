#pragma once
#include <QString>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

class CurveSegmentModel;

class CurveSegmentFactory
{
    public:
        virtual QString name() const = 0;
        virtual CurveSegmentModel* make(
                const id_type<CurveSegmentModel>&,
                QObject* parent) = 0;

        virtual CurveSegmentModel *load(
                const VisitorVariant& data,
                QObject* parent) = 0;
};
