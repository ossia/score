#pragma once
#include <boost/optional/optional.hpp>
#include <QVariant>


#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
struct SegmentData;
struct ISCORE_PLUGIN_CURVE_EXPORT PowerSegmentData
{
        double gamma;
};

class ISCORE_PLUGIN_CURVE_EXPORT PowerSegment final : public SegmentModel
{
    public:
        using data_type = PowerSegmentData;
        using SegmentModel::SegmentModel;
        PowerSegment(
                const SegmentData& dat,
                QObject* parent);

        template<typename Impl>
        PowerSegment(Deserializer<Impl>& vis, QObject* parent) :
            SegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        double gamma = 12.05; // TODO private
    private:
        SegmentModel* clone(
                const Id<SegmentModel>& id,
                QObject* parent) const override;

        UuidKey<Curve::SegmentFactory> concreteFactoryKey() const override;
        void serialize_impl(const VisitorVariant& vis) const override;
        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) const override;
        double valueAt(double x) const override;

        boost::optional<double> verticalParameter() const override;
        void setVerticalParameter(double p) override;

        QVariant toSegmentSpecificData() const override
        {
            return QVariant::fromValue(PowerSegmentData{gamma});
        }

};
}
CURVE_SEGMENT_METADATA(
        ISCORE_PLUGIN_CURVE_EXPORT,
        Curve::PowerSegment,
        "1e7cb83f-4e47-4b14-814d-2242a9c75991",
        "Power",
        "Power")

Q_DECLARE_METATYPE(Curve::PowerSegmentData)
