#pragma once
#include "Curve/Segment/CurveSegmentModel.hpp"

class SinCurveSegmentModel : public CurveSegmentModel
{
    public:
        using CurveSegmentModel::CurveSegmentModel;

        template<typename Impl>
        SinCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
            CurveSegmentModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        CurveSegmentModel* clone(
                const Id<CurveSegmentModel>& id,
                QObject* parent) const override;

        QString name() const override;
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

};
