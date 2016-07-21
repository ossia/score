#pragma once
#include <iscore/tools/std/Optional.hpp>
#include <QVariant>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

#include <iscore/tools/SettableIdentifier.hpp>

namespace Curve
{
class PowerSegment;
}

CURVE_SEGMENT_METADATA(
        ISCORE_PLUGIN_CURVE_EXPORT,
        Curve::PowerSegment,
        "1e7cb83f-4e47-4b14-814d-2242a9c75991",
        "Power",
        "Power")

namespace Curve
{
struct SegmentData;
struct ISCORE_PLUGIN_CURVE_EXPORT PowerSegmentData
{
        PowerSegmentData() = default;
        PowerSegmentData(double d):
            gamma{d}
        {

        }


        // Value of gamma for which the pow will be == 1.
        static const constexpr double linearGamma = 11.05;
        double gamma = linearGamma;
};

class ISCORE_PLUGIN_CURVE_EXPORT PowerSegment final :
        public SegmentModel
{
        MODEL_METADATA_IMPL(PowerSegment)
    public:
        using data_type = PowerSegmentData;
        using SegmentModel::SegmentModel;
        PowerSegment(
                const SegmentData& dat,
                QObject* parent);

        PowerSegment(
                const PowerSegment& other,
                const id_type& id,
                QObject* parent):
            SegmentModel{other.start(), other.end(), id, parent},
            gamma{other.gamma}
        {
        }

        template<typename Impl>
        PowerSegment(Deserializer<Impl>& vis, QObject* parent) :
             SegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        double gamma = PowerSegmentData::linearGamma; // TODO private
    private:
        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) const override;
        double valueAt(double x) const override;

        optional<double> verticalParameter() const override;
        void setVerticalParameter(double p) override;

        QVariant toSegmentSpecificData() const override
        {
            return QVariant::fromValue(PowerSegmentData(gamma));
        }

        template<typename Y>
        std::function<Y(double, Y, Y)> makeFunction() const
        {
            if(gamma == Curve::PowerSegmentData::linearGamma)
            {
                // We just return the linear one
                return [] (double ratio, Y start, Y end) {
                    return start + ratio * (end - start);
                };
            }
            else
            {
                double thepow = Curve::PowerSegmentData::linearGamma + 1 - gamma;
                return [=] (double ratio, Y start, Y end) {
                    return start + std::pow(ratio, thepow) * (end - start);
                };
            }
        }
        std::function<float(double, float, float)> makeFloatFunction() const override
        { return makeFunction<float>(); }
        std::function<int(double, int, int)> makeIntFunction() const override
        { return makeFunction<int>(); }
        std::function<bool(double, bool, bool)> makeBoolFunction() const override
        { return makeFunction<bool>(); }
};
}

Q_DECLARE_METATYPE(Curve::PowerSegmentData)
