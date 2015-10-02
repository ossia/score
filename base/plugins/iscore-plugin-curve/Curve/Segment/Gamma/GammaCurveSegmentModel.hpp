#pragma once
#include "Curve/Segment/CurveSegmentModel.hpp"

struct GammaCurveSegmentData
{
    double gamma;
};

Q_DECLARE_METATYPE(GammaCurveSegmentData)

class GammaCurveSegmentModel : public CurveSegmentModel
{
    public:
        using data_type = GammaCurveSegmentData;
        using CurveSegmentModel::CurveSegmentModel;
        GammaCurveSegmentModel(
                const CurveSegmentData& dat,
                QObject* parent);

        template<typename Impl>
        GammaCurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
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
            return QVariant::fromValue(GammaCurveSegmentData{gamma});
        }

        double gamma = 0.5;

};
