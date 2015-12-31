#pragma once
#include <boost/optional/optional.hpp>
#include <QVariant>

#include <Curve/Segment/CurveSegmentFactoryKey.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
struct SegmentData;
struct ISCORE_PLUGIN_CURVE_EXPORT PowerSegmentData
{
        static const SegmentFactoryKey& key();
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

        SegmentModel* clone(
                const Id<SegmentModel>& id,
                QObject* parent) const override;

        const SegmentFactoryKey& key() const override;
        void serialize(const VisitorVariant& vis) const override;
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

        double gamma = 12.05; // TODO private
};
}
Q_DECLARE_METATYPE(Curve::PowerSegmentData)
