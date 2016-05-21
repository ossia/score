#pragma once
#include <iscore/tools/std/Optional.hpp>
#include <QVariant>


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

class GammaSegment final :
        public Segment<GammaSegment>
{
    public:
        using data_type = GammaSegmentData;
        using Segment<GammaSegment>::Segment;
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

        void serialize_impl(const VisitorVariant& vis) const override;
        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) const override;
        double valueAt(double x) const override;

        optional<double> verticalParameter() const override;
        void setVerticalParameter(double p) override;

        QVariant toSegmentSpecificData() const override
        {
            return QVariant::fromValue(GammaSegmentData{gamma});
        }

        double gamma = 0.5;
};
}
CURVE_SEGMENT_METADATA(
        ISCORE_PLUGIN_CURVE_EXPORT,
        Curve::GammaSegment,
        "ece054f5-508e-48ca-a1d2-c581aa922e23",
        "Gamma",
        "Gamma")

Q_DECLARE_METATYPE(Curve::GammaSegmentData)
