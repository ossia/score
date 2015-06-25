#pragma once
#include <QString>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

class CurveSegmentModel;

class CurveSegmentFactory : public iscore::FactoryInterface
{
    public:
        static QString factoryName();
        virtual QString name() const = 0;
        virtual CurveSegmentModel* make(
                const id_type<CurveSegmentModel>&,
                QObject* parent) = 0;

        virtual CurveSegmentModel *load(
                const VisitorVariant& data,
                QObject* parent) = 0;
};
