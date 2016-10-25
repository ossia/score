#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <ossia/editor/curve/curve_segment/linear.hpp>

namespace Curve { class LinearSegment; }

CURVE_SEGMENT_METADATA(
        ISCORE_PLUGIN_CURVE_EXPORT,
        Curve::LinearSegment,
        "a8bd14e2-d7e4-47cd-b76a-6a88fa11f0d2",
        "Linear",
        "Linear")

namespace Curve
{
struct ISCORE_PLUGIN_CURVE_EXPORT LinearSegmentData
{
};

class ISCORE_PLUGIN_CURVE_EXPORT LinearSegment final :
        public SegmentModel
{
        MODEL_METADATA_IMPL(LinearSegment)
    public:
        using data_type = LinearSegmentData;
        using SegmentModel::SegmentModel;

        LinearSegment(
                const LinearSegment& other,
                const id_type& id,
                QObject* parent):
            SegmentModel{other.start(), other.end(), id, parent}
        {
        }

        template<typename Impl>
        LinearSegment(Deserializer<Impl>& vis, QObject* parent) :
            SegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) const override;
        double valueAt(double x) const override;

        QVariant toSegmentSpecificData() const override;

        std::function<float(double, float, float)> makeFloatFunction() const override
        { return ossia::curve_segment_linear<float>{}; }
        std::function<int(double, int, int)> makeIntFunction() const override
        { return ossia::curve_segment_linear<int>{}; }
        std::function<bool(double, bool, bool)> makeBoolFunction() const override
        { return ossia::curve_segment_linear<bool>{}; }
};
}

Q_DECLARE_METATYPE(Curve::LinearSegmentData)
