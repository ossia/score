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
struct SinSegmentData
{
    double freq;
    double ampl;
};

class ISCORE_PLUGIN_CURVE_EXPORT SinSegment final :
        public Segment<SinSegment>
{
    public:
        using data_type = SinSegmentData;
        using Segment<SinSegment>::Segment;
        SinSegment(
                const SegmentData& dat,
                QObject* parent);

        template<typename Impl>
        SinSegment(Deserializer<Impl>& vis, QObject* parent) :
            Segment<SinSegment> {vis, parent}
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
        optional<double> horizontalParameter() const override;
        void setVerticalParameter(double p) override;
        void setHorizontalParameter(double p) override;

        double freq = 5;
        double ampl = 0.6;

        QVariant toSegmentSpecificData() const override
        {
            return QVariant::fromValue(SinSegmentData{freq, ampl});
        }

};
}
CURVE_SEGMENT_METADATA(
        ISCORE_PLUGIN_CURVE_EXPORT,
        Curve::SinSegment,
        "c16dad6a-a422-4fb7-8bd5-850cbe8c3f28",
        "Sin",
        "Sin")

Q_DECLARE_METATYPE(Curve::SinSegmentData)
