#pragma once
#include "Curve/Segment/CurveSegmentModel.hpp"

struct SinCurveSegmentData
{
    double freq;
    double ampl;
};

Q_DECLARE_METATYPE(SinCurveSegmentData)

class SinCurveSegmentModel final : public CurveSegmentModel
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

        const std::string& name() const override;
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
