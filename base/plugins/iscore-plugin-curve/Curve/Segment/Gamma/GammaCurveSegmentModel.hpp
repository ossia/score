#pragma once
#include <boost/optional/optional.hpp>
#include <QVariant>

#include <Curve/Segment/CurveSegmentFactoryKey.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
struct CurveSegmentData;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
struct GammaSegmentData
{
    double gamma;
};

class GammaSegment final : public SegmentModel
{
    public:
        using data_type = GammaSegmentData;
        using SegmentModel::SegmentModel;
        GammaSegment(
                const SegmentData& dat,
                QObject* parent);

        template<typename Impl>
        GammaSegment(Deserializer<Impl>& vis, QObject* parent) :
            SegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        SegmentModel* clone(
                const Id<SegmentModel>& id,
                QObject* parent) const override;

        SegmentFactoryKey concreteFactoryKey() const override;
        void serialize_impl(const VisitorVariant& vis) const override;
        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) const override;
        double valueAt(double x) const override;

        boost::optional<double> verticalParameter() const override;
        void setVerticalParameter(double p) override;

        QVariant toSegmentSpecificData() const override
        {
            return QVariant::fromValue(GammaSegmentData{gamma});
        }

        double gamma = 0.5;
};
}

Q_DECLARE_METATYPE(Curve::GammaSegmentData)
