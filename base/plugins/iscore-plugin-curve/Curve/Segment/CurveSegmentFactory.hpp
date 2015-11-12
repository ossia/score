#pragma once
#include <QString>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

class CurveSegmentModel;

class CurveSegmentFactory : public iscore::GenericFactoryInterface<std::string>
{
        ISCORE_FACTORY_DECL("CurveSegment")
    public:

        virtual ~CurveSegmentFactory();

        virtual CurveSegmentModel* make(
                const Id<CurveSegmentModel>&,
                QObject* parent) = 0;

        virtual CurveSegmentModel* load(
                const VisitorVariant& data,
                QObject* parent) = 0;

        virtual CurveSegmentModel* load(
                const CurveSegmentData& data,
                QObject* parent) = 0;

        virtual QVariant makeCurveSegmentData() const = 0;

        virtual void serializeCurveSegmentData(
                const QVariant& data,
                const VisitorVariant& visitor) const = 0;
        virtual QVariant makeCurveSegmentData(
                const VisitorVariant& visitor) const = 0;
};

template<typename T>
class CurveSegmentFactory_T : public CurveSegmentFactory
{
    public:
        CurveSegmentModel *make(
                const Id<CurveSegmentModel>& id,
                QObject* parent) override
        { return new T{id, parent}; }

        CurveSegmentModel *load(
                const VisitorVariant& vis,
                QObject* parent) override
        {
            return deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new T{deserializer, parent}; });
        }

        CurveSegmentModel *load(
                const CurveSegmentData& dat,
                QObject* parent) override
        { return new T{dat, parent}; }

        QVariant makeCurveSegmentData() const override
        { return QVariant::fromValue(typename T::data_type{}); }

        void serializeCurveSegmentData(
                const QVariant& data,
                const VisitorVariant& visitor) const override
        { serialize_dyn(visitor, data.value<typename T::data_type>()); }

        QVariant makeCurveSegmentData(
                const VisitorVariant& vis) const override
        { return QVariant::fromValue(deserialize_dyn<typename T::data_type>(vis)); }
};

#define DEFINE_CURVE_SEGMENT_FACTORY(Name, DynName, Model) \
    class Name final : public CurveSegmentFactory_T<Model> \
{ \
    const std::string& key_impl() const override { \
        static const std::string name{DynName}; \
        return name; \
} \
};
