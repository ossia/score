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
struct SinSegmentData
{
    double freq;
    double ampl;


    static const QString prettyName()
    { return QObject::tr("Sin"); }

    static const SegmentFactoryKey& static_concreteFactoryKey();
};

class ISCORE_PLUGIN_CURVE_EXPORT SinSegment final : public SegmentModel
{
    public:
        using data_type = SinSegmentData;
        using SegmentModel::SegmentModel;
        SinSegment(
                const SegmentData& dat,
                QObject* parent);

        template<typename Impl>
        SinSegment(Deserializer<Impl>& vis, QObject* parent) :
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
        boost::optional<double> horizontalParameter() const override;
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

Q_DECLARE_METATYPE(Curve::SinSegmentData)
