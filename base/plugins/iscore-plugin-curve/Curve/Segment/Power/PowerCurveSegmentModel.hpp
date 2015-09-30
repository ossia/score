#pragma once
#include "Curve/Segment/CurveSegmentModel.hpp"

struct PowerCurveSegmentData
{
    double gamma;
};

Q_DECLARE_METATYPE(PowerCurveSegmentData)

class PowerCurveSegmentModel : public CurveSegmentModel
{
    public:
        using data_type = PowerCurveSegmentData;
        using CurveSegmentModel::CurveSegmentModel;
        PowerCurveSegmentModel(
                const CurveSegmentData& dat,
                QObject* parent);

        template<typename Impl>
        PowerCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
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
        void setVerticalParameter(double p) override;

        QVariant toSegmentSpecificData() const
        {
            return QVariant::fromValue(PowerCurveSegmentData{gamma});
        }

        double gamma = 0.5; // TODO private
};
