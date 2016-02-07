#pragma once
#include <QVariant>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
struct ISCORE_PLUGIN_CURVE_EXPORT LinearSegmentData
{
};

class ISCORE_PLUGIN_CURVE_EXPORT LinearSegment final :
        public Segment<LinearSegment>
{
    public:
        using data_type = LinearSegmentData;
        using Segment<LinearSegment>::Segment;

        template<typename Impl>
        LinearSegment(Deserializer<Impl>& vis, QObject* parent) :
            Segment<LinearSegment> {vis, parent}
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

        QVariant toSegmentSpecificData() const override;
};
}

CURVE_SEGMENT_METADATA(
        ISCORE_PLUGIN_CURVE_EXPORT,
        Curve::LinearSegment,
        "a8bd14e2-d7e4-47cd-b76a-6a88fa11f0d2",
        "Linear",
        "Linear")

Q_DECLARE_METATYPE(Curve::LinearSegmentData)
