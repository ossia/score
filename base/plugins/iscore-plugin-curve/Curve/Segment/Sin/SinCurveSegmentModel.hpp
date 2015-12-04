#pragma once
#include <boost/optional/optional.hpp>
#include <QVariant>

#include <Curve/Segment/CurveSegmentFactoryKey.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
struct CurveSegmentData;
#include <iscore/tools/SettableIdentifier.hpp>

struct SinCurveSegmentData
{
    double freq;
    double ampl;
};

Q_DECLARE_METATYPE(SinCurveSegmentData)

class ISCORE_PLUGIN_CURVE_EXPORT SinCurveSegmentModel final : public CurveSegmentModel
{
    public:
        using data_type = SinCurveSegmentData;
        using CurveSegmentModel::CurveSegmentModel;
        SinCurveSegmentModel(
                const CurveSegmentData& dat,
                QObject* parent);

        template<typename Impl>
        SinCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveSegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        CurveSegmentModel* clone(
                const Id<CurveSegmentModel>& id,
                QObject* parent) const override;

        const CurveSegmentFactoryKey& key() const override;
        void serialize(const VisitorVariant& vis) const override;
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
            return QVariant::fromValue(SinCurveSegmentData{freq, ampl});
        }

};
