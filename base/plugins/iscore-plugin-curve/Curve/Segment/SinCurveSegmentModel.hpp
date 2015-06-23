#pragma once
#include "CurveSegmentModel.hpp"

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
                const id_type<CurveSegmentModel>& id,
                QObject* parent) const override;

        QString name() const override;
        void serialize(const VisitorVariant& vis) const override;
        void on_startChanged() override;
        void on_endChanged() override;

        void updateData(int numInterp) override;

        boost::optional<double> verticalParameter();
        boost::optional<double> horizontalParameter();
        void setVerticalParameter(double p);
        void setHorizontalParameter(double p);

        double freq = 5;
        double ampl = 0.6;

};
